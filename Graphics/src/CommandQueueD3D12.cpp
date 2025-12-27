#include "pch.h"
#include "CommandQueueD3D12.h"
#include "RenderDeviceD3D12.h"

#include <MemoryAllocatorT.h>

namespace EduEngine
{
	CommandQueueD3D12::CommandQueueD3D12(RenderDeviceD3D12* pDevice, D3D12_COMMAND_LIST_TYPE type) :
		m_NextCmdList(0)
	{
		VERIFY_EXPR(CmdListTypeToQueueId(type), "Unsupported command queue type");

		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = type;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		pDevice->GetD3D12Device()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_CommandQueue));
		pDevice->GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_Fence));

		m_CommandQueue->SetName(type == D3D12_COMMAND_LIST_TYPE_DIRECT ? L"DirectCommandQueue" : L"ComputeCommandQueue");
	}

	CommandQueueD3D12::~CommandQueueD3D12()
	{
		ProcessReleaseQueue(true);
	}

	void CommandQueueD3D12::CloseAndExecuteCommandContexts(CommandContext** commandContexts, uint32 numContexts)
	{
		constexpr int NumStaticCmdList = 16;

		ID3D12CommandList* cmdListsStatic[NumStaticCmdList];
		std::vector<ID3D12CommandList*, MemoryAllocator::MemoryAllocatorT<ID3D12CommandList*>> cmdListsDynamic;

		ID3D12CommandList** cmdLists = nullptr;

		if (numContexts <= NumStaticCmdList)
		{
			cmdLists = cmdListsStatic;
		}
		else
		{
			cmdListsDynamic.resize(numContexts);
			cmdLists = cmdListsDynamic.data();
		}

		for (uint32 i = 0; i < numContexts; i++)
		{
			assert(m_CommandQueue->GetDesc().Type == commandContexts[i]->GetType());

			commandContexts[i]->FlushResourceBarriers();

			cmdLists[i] = commandContexts[i]->Close();
		}

		std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);

		if (numContexts)
			m_CommandQueue->ExecuteCommandLists(numContexts, cmdLists);

		uint64 FenceValue = m_NextCmdList.fetch_add(1);

		m_CommandQueue->Signal(m_Fence.Get(), FenceValue);

		for (uint32 i = 0; i < numContexts; i++)
		{
			commandContexts[i]->DiscardAllocator(FenceValue);
		}
	}

	void CommandQueueD3D12::Signal()
	{
		uint64 FenceValue = m_NextCmdList.fetch_add(1);
		m_CommandQueue->Signal(m_Fence.Get(), FenceValue);
	}

	void CommandQueueD3D12::Wait(CommandQueueD3D12* other, UINT64 fenceValue)
	{
		m_CommandQueue->Wait(other->m_Fence.Get(), fenceValue);
	}

	void CommandQueueD3D12::SafeReleaseObject(ReleaseResource staleObject)
	{
		m_ReleaseObjectsQueue.emplace_back(m_NextCmdList.load(), std::move(staleObject));
	}

	void CommandQueueD3D12::ProcessReleaseQueue(bool forceRelease)
	{
		std::lock_guard<std::mutex> LockGuard(m_ReleasedObjectsMutex);

		auto numCompletedCmdLists = GetCompletedFenceNum();

		while (!m_ReleaseObjectsQueue.empty())
		{
			auto& firstObj = m_ReleaseObjectsQueue.front();
			// GPU must have been idled when ForceRelease == true 
			if (firstObj.first < numCompletedCmdLists || forceRelease)
				m_ReleaseObjectsQueue.pop_front();
			else
				break;
		}

		auto nextCmdList = m_NextCmdList.load();
	}

	void CommandQueueD3D12::Flush()
	{
		m_NextCmdList++;

		// Add an instruction to the command queue to set a new fence point.  Because we 
		// are on the GPU timeline, the new fence point won't be set until the GPU finishes
		// processing all the commands prior to this Signal().
		HRESULT hr = m_CommandQueue->Signal(m_Fence.Get(), m_NextCmdList);
		THROW_IF_FAILED(hr, L"Failed to signal command queue");

		// Wait until the GPU has completed commands up to this fence point.
		if (m_Fence->GetCompletedValue() < m_NextCmdList)
		{
			HANDLE eventHandle = CreateEventEx(nullptr, FALSE, false, EVENT_ALL_ACCESS);

			// fire event when GPU hits current fence  
			hr = m_Fence->SetEventOnCompletion(m_NextCmdList, eventHandle);
			THROW_IF_FAILED(hr, L"Failed to set event on completion");

			// wait until the GPU hits current fence event is fired
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}

#ifdef _DEBUG
	std::string CommandQueueD3D12::GetDebugReleaseQueueStr()
	{
		std::string result;

		result += "CommandQueueType: ";

		switch (m_CommandQueue->GetDesc().Type)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			result += "Direct\n";
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			result += "Compute\n";
			break;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			result += "Copy\n";
			break;
		default:
			result += "Unknown\n";
		}

		for (auto& obj : m_ReleaseObjectsQueue)
		{
			result += std::to_string(obj.first);
			result += " -- ";
			result += obj.second.GetReleaseResourceDebugStr();
			result += "\n";
		}

		return result;
	}
#endif
}