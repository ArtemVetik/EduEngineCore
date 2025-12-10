#pragma once
#include "framework.h"
#include "CommandQueueD3D12.h"
#include "DeviceContext.h"
#include "QueryHeap.h"

#include <IRenderDeviceD3D12.h>
#include <CPUDescriptorHeap.h>
#include <GPUDescriptorHeap.h>
#include <ReleaseResourceWrapper.h>
#include <QueueMask.h>

namespace EduEngine
{
	class GRAPHICS_API RenderDeviceD3D12 : public IRenderDeviceD3D12
	{
	public:
		RenderDeviceD3D12(Microsoft::WRL::ComPtr<ID3D12Device> device);
		~RenderDeviceD3D12();

		RenderDeviceD3D12(const RenderDeviceD3D12&) = delete;
		RenderDeviceD3D12(RenderDeviceD3D12&&) = delete;
		RenderDeviceD3D12& operator = (const RenderDeviceD3D12&) = delete;
		RenderDeviceD3D12& operator = (RenderDeviceD3D12&&) = delete;

		DescriptorHeapAllocation AllocateCPUDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count);
		DescriptorHeapAllocation AllocateGPUDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count);

		CommandQueueD3D12& GetCommandQueue(D3D12_COMMAND_LIST_TYPE type);
		const QueryHeap& GetQueryHeap() const { return m_QueryHeap; }

		virtual void SafeReleaseObject(ReleaseResourceWrapper&& wrapper) override;
		void FinishFrame(bool forceRelease = false);
		
		void FlushQueues();


		ID3D12DescriptorHeap* GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) const;
		
		ID3D12Device* GetD3D12Device() const override { return mDevice.Get(); }

		static constexpr uint32 MaxDeviceContexts = 32;

	private:
		GPUDescriptorHeap& GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
		DynamicHeapManager& GetDynamicHeapManager() { return m_GlobalDynamicHeap; }
		uint32 GetAvailableContextId();
		
		friend DeviceContext::DeviceContext(RenderDeviceD3D12& device, D3D12_COMMAND_LIST_TYPE type);
		
	private:
		Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
		CPUDescriptorHeap m_CPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		GPUDescriptorHeap m_GPUDescriptorHeaps[2];

		typedef std::pair<FenceValues, ReleaseResourceWrapper> ReleaseObject;

		std::mutex m_ReleasedObjectsMutex;
		DynamicHeapManager m_GlobalDynamicHeap; // must be before m_ReleaseObjectsQueue

		CommandQueueD3D12 m_CommandQueues[3]; // must be after descriptor heaps (release in destructor)
		QueryHeap m_QueryHeap;

		std::vector<uint32> m_AvailableContextIds;
	};
}