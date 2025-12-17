#pragma once
#include "framework.h"
#include "CommandQueueD3D12.h"
#include "CommandContextPool.h"
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
		RenderDeviceD3D12(Microsoft::WRL::ComPtr<ID3D12Device> device, QueueMask commandQueues);
		~RenderDeviceD3D12();

		RenderDeviceD3D12(const RenderDeviceD3D12&) = delete;
		RenderDeviceD3D12(RenderDeviceD3D12&&) = delete;
		RenderDeviceD3D12& operator = (const RenderDeviceD3D12&) = delete;
		RenderDeviceD3D12& operator = (RenderDeviceD3D12&&) = delete;

		DescriptorHeapAllocation AllocateCPUDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count);
		DescriptorHeapAllocation AllocateGPUDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count);

		CommandQueueD3D12& GetCommandQueue(D3D12_COMMAND_LIST_TYPE type);
		CommandContextPool& GetCommandContextPool() { return m_CmdContextPool; }
		const QueryHeap& GetQueryHeap() const { return *m_QueryHeap; }

		virtual void SafeReleaseObject(ReleaseResourceWrapper&& wrapper, QueueMask queueMask) override;
		void FinishFrame(bool forceRelease = false);
		
		void FlushQueues();


		ID3D12DescriptorHeap* GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) const;
		
		ID3D12Device* GetD3D12Device() const override { return mDevice.Get(); }

	private:
		CommandQueueD3D12& GetCommandQueue(QueueId queueId);

		GPUDescriptorHeap& GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
		DynamicHeapManager& GetDynamicHeapManager() { return m_GlobalDynamicHeap; }
		uint32 AllocateContextId();
		void FreeContextId(uint32 id);
		
		friend DeviceContext::DeviceContext(RenderDeviceD3D12& device, const DeviceContextDesc& desc);
		friend DeviceContext::~DeviceContext();
		
	private:
		Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
		CPUDescriptorHeap m_CPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		GPUDescriptorHeap m_GPUDescriptorHeaps[2];

		std::mutex m_ReleasedObjectsMutex;
		DynamicHeapManager m_GlobalDynamicHeap; // must be before m_ReleaseObjectsQueue

		uint8 m_QueueCount;
		QueueMask m_ActiveQueues;
		CommandQueueD3D12* m_CommandQueues; // must be released before descriptor heaps
		QueryHeap* m_QueryHeap;

		CommandContextPool m_CmdContextPool;

		uint32 m_NextAviableContextId;
		std::vector<uint32> m_AvailableContextIds;
	};
}