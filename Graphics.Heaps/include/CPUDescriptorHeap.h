#pragma once
#include "framework.h"
#include "DescriptorHeapAllocationManager.h"
#include "ReleaseResourceWrapper.h"

#include <set>

namespace EduEngine
{
	class GRAPHICS_HEAPS_API CPUDescriptorHeap: public IDescriptorAllocator
	{
	public:
		CPUDescriptorHeap(IRenderDeviceD3D12&		  deviceD3D12Impl,
						  uint32					  numDescriptorsInHeap,
						  D3D12_DESCRIPTOR_HEAP_TYPE  type,
						  D3D12_DESCRIPTOR_HEAP_FLAGS flags);

		CPUDescriptorHeap(const CPUDescriptorHeap&) = delete;
		CPUDescriptorHeap(CPUDescriptorHeap&&) = delete;
		CPUDescriptorHeap& operator = (const CPUDescriptorHeap&) = delete;
		CPUDescriptorHeap& operator = (CPUDescriptorHeap&&) = delete;

		~CPUDescriptorHeap();

		virtual DescriptorHeapAllocation Allocate(uint32 count) override;
		virtual void SafeFree(DescriptorHeapAllocation&& allocation, QueueMask queueMask) override;
		virtual uint32 GetDescriptorSize() const override { return m_DescriptorSize; }
		virtual void FreeAllocation(DescriptorHeapAllocation&& allocation) override;

	private:
		std::mutex m_HeapPoolMutex;
		std::vector<DescriptorHeapAllocationManager> m_HeapPool;
		std::set<size_t> m_AvailableHeaps;

		IRenderDeviceD3D12& m_DeviceD3D12Impl;
		D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
		const UINT m_DescriptorSize = 0;

		uint32 m_MaxSize = 0;
		uint32 m_CurrentSize = 0;
	};
}