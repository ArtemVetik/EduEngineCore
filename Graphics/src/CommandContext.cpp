#include "pch.h"
#include "CommandContext.h"
#include "ResourceD3D12.h"

namespace EduEngine
{
	CommandContext::CommandContext(RenderDeviceD3D12& pDevice, CommandListManager& cmdListMgr) :
        m_CommandListManager(cmdListMgr)
	{
        m_CommandListManager.CreateNewCommandList(m_pCommandList.GetAddressOf(), m_pCurrentAllocator.GetAddressOf());
	}

	void CommandContext::Reset()
	{
        assert(m_pCommandList != nullptr);

        if (!m_pCurrentAllocator)
        {
            m_CommandListManager.RequestCommandAllocator(&m_pCurrentAllocator);
            // Unlike ID3D12CommandAllocator::Reset, ID3D12GraphicsCommandList::Reset can be called while the
            // command list is still being executed. A typical pattern is to submit a command list and then
            // immediately reset it to reuse the allocated memory for another command list.
            m_pCommandList->Reset(m_pCurrentAllocator.Get(), nullptr);
        }
	}

    ID3D12GraphicsCommandList* CommandContext::Close()
    {
        assert(m_pCurrentAllocator != nullptr);
        auto hr = m_pCommandList->Close();
        
        assert(SUCCEEDED(hr));

        return m_pCommandList.Get();
    }

    void CommandContext::SetViewports(const D3D12_VIEWPORT* viewports, size_t count) const
    {
        assert(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
        m_pCommandList->RSSetViewports(count, viewports);
    }

    void CommandContext::SetScissorRects(const D3D12_RECT* scissorRects, size_t count) const
    {
        assert(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
        m_pCommandList->RSSetScissorRects(count, scissorRects);
    }

    void CommandContext::SetRenderTargets(UINT num, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvView, BOOL isSingleHandle, const D3D12_CPU_DESCRIPTOR_HANDLE* dsvView) const
    {
        m_pCommandList->OMSetRenderTargets(num, rtvView, isSingleHandle, dsvView);
    }

    void CommandContext::UpdateSubresource(ID3D12Resource* dest, ID3D12Resource* intermediate, D3D12_SUBRESOURCE_DATA* pSrcData)
    {
        UpdateSubresources<1>(m_pCommandList.Get(), dest, intermediate, 0, 0, 1, pSrcData);
    }

    void CommandContext::TransitionResource(ResourceD3D12* resource, D3D12_RESOURCE_STATES newState, bool flushImmediate)
    {
        D3D12_RESOURCE_STATES oldState = resource->GetState();

        // Check if required state is already set
        if ((oldState & newState) != newState)
        {
            // If both old state and new state are read-only states, combine the two
            if ((oldState & D3D12_RESOURCE_STATE_GENERIC_READ) == oldState &&
                (newState & D3D12_RESOURCE_STATE_GENERIC_READ) == newState)
                newState |= oldState;

            m_PendingResourceBarriers.emplace_back();
            D3D12_RESOURCE_BARRIER& barrierDesc = m_PendingResourceBarriers.back();

            barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierDesc.Transition.pResource = resource->GetD3D12Resource();
            barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrierDesc.Transition.StateBefore = oldState;
            barrierDesc.Transition.StateAfter = newState;
            barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            resource->SetState(newState);
        }
        else if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            InsertUAVBarrier(resource, flushImmediate);

        if (flushImmediate || m_PendingResourceBarriers.size() >= MaxPendingBarriers)
            FlushResourceBarriers();
    }

    void CommandContext::InsertUAVBarrier(ResourceD3D12* resource, bool flushImmediate)
    {
        m_PendingResourceBarriers.emplace_back();
        D3D12_RESOURCE_BARRIER& barrierDesc = m_PendingResourceBarriers.back();

        barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrierDesc.UAV.pResource = resource->GetD3D12Resource();

        if (flushImmediate)
            FlushResourceBarriers();
    }

    void CommandContext::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
    {
        m_PendingResourceBarriers.push_back(barrier);
    }

    void CommandContext::FlushResourceBarriers()
    {
        if (!m_PendingResourceBarriers.empty())
        {
            m_pCommandList->ResourceBarrier(static_cast<UINT>(m_PendingResourceBarriers.size()), m_PendingResourceBarriers.data());
            m_PendingResourceBarriers.clear();
        }
    }

    void CommandContext::DiscardAllocator(uint64_t fenceValue)
    {
        m_CommandListManager.DiscardAllocator(fenceValue, m_pCurrentAllocator);
    }

    D3D12_COMMAND_LIST_TYPE CommandContext::GetType() const
    {
        return m_CommandListManager.GetType();
    }
}