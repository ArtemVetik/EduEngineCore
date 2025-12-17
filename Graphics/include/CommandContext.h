#pragma once
#include "framework.h"
#include "CommandListManager.h"

#include <vector>

namespace EduEngine
{
	class GRAPHICS_API CommandContext
	{
	public:
		CommandContext(RenderDeviceD3D12& pDevice, CommandListManager& cmdListMgr);
		
		CommandContext(const CommandContext&) = delete;
		CommandContext(CommandContext&&) = delete;
		CommandContext& operator = (const CommandContext&) = delete;
		CommandContext& operator = (CommandContext&&) = delete;

		void Reset();
		ID3D12GraphicsCommandList* Close();

		void SetViewports(const D3D12_VIEWPORT* viewports, size_t count) const;
		void SetScissorRects(const D3D12_RECT* scissorRects, size_t count) const;
		void SetRenderTargets(UINT num, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvView, BOOL isSingleHandle, const D3D12_CPU_DESCRIPTOR_HANDLE* dsvView) const;

		void UpdateSubresource(ID3D12Resource* dest, ID3D12Resource* intermediate, D3D12_SUBRESOURCE_DATA* pSrcData);

		void TransitionResource(class ResourceD3D12* resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false);
		void InsertUAVBarrier(class ResourceD3D12* resource, bool flushImmediate = false);
		void ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);
		void FlushResourceBarriers();

		void DiscardAllocator(uint64_t fenceValue);

		D3D12_COMMAND_LIST_TYPE GetType() const;

	private:
		static const int MaxPendingBarriers = 16;

		CommandListManager& m_CommandListManager;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_pCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_pCurrentAllocator;

		std::vector<D3D12_RESOURCE_BARRIER> m_PendingResourceBarriers;

	public:
		ID3D12GraphicsCommandList4* GetCmdList() const { return m_pCommandList.Get(); }
	};
}