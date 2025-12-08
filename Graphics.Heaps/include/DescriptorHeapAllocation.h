#pragma once
#include "framework.h"
#include "QueueMask.h"

namespace EduEngine
{
	class GRAPHICS_HEAPS_API DescriptorHeapAllocation;
	class GRAPHICS_HEAPS_API IDescriptorAllocator;

	class GRAPHICS_HEAPS_API IDescriptorAllocator
	{
	public:
		virtual DescriptorHeapAllocation Allocate(QueueMask queueMask, uint32 count) = 0;
		virtual void SafeFree(DescriptorHeapAllocation&& allocation) = 0;
		virtual uint32 GetDescriptorSize() const = 0;
		virtual void FreeAllocation(DescriptorHeapAllocation&& allocation) = 0;

		virtual ~IDescriptorAllocator() {}
	};

	class GRAPHICS_HEAPS_API DescriptorHeapAllocation
	{
	public:
		DescriptorHeapAllocation();

		DescriptorHeapAllocation(IDescriptorAllocator&		 allocator,
								 ID3D12DescriptorHeap*		 heap,
								 D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
								 D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
								 uint32						 nHandles,
								 uint16						 allocationManagerId,
								 QueueMask					 queueMask);

		DescriptorHeapAllocation(DescriptorHeapAllocation&& allocation) noexcept;

		DescriptorHeapAllocation& operator = (DescriptorHeapAllocation&& allocation) noexcept;

		~DescriptorHeapAllocation();

		void Reset();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32 offset = 0) const;

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32 offset = 0) const;

		ID3D12DescriptorHeap* GetDescriptorHeap() { return m_pDescriptorHeap; }

		size_t GetNumHandles()const { return m_NumHandles; }

		bool IsNull() const { return m_FirstCpuHandle.ptr == 0; }
		bool IsShaderVisible() const { return m_FirstGpuHandle.ptr != 0; }
		size_t GetAllocationManagerId() { return m_AllocationManagerId; }
		uint16 GetDescriptorSize() const { return m_DescriptorSize; }
		QueueMask GetQueueMask() const { return m_QueueMask; }

		static constexpr uint16 InvalidAllocationMgrId = 0xFFFF;

	private:
		DescriptorHeapAllocation(const DescriptorHeapAllocation&) = delete;
		DescriptorHeapAllocation& operator= (const DescriptorHeapAllocation&) = delete;

		D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCpuHandle = { 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGpuHandle = { 0 };
		IDescriptorAllocator* m_pAllocator = nullptr;
		ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
		uint32 m_NumHandles = 0;
		uint16 m_AllocationManagerId = static_cast<uint16>(-1);
		uint16 m_DescriptorSize = 0;
		QueueMask m_QueueMask;
	};
}