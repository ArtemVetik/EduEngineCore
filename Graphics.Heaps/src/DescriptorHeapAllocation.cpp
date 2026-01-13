#include "pch.h"
#include "DescriptorHeapAllocation.h"

namespace EduEngine
{
	DescriptorHeapAllocation::DescriptorHeapAllocation() : 
        m_pDescriptorHeap{ nullptr },
        m_NumHandles{ 1 },
        m_DescriptorSize{ 0 },
        m_HeapOffset{ 0 }
    {
        m_FirstCpuHandle.ptr = 0;
        m_FirstGpuHandle.ptr = 0;
    }

    DescriptorHeapAllocation::DescriptorHeapAllocation(IDescriptorAllocator&       allocator,
                                                       ID3D12DescriptorHeap*       heap,
                                                       D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                                                       D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
                                                       uint64                      heapOffset,
                                                       uint32                      nHandles,
                                                       uint16                      allocationManagerId) :
        m_FirstCpuHandle{ cpuHandle },
        m_FirstGpuHandle{ gpuHandle },
        m_HeapOffset{ heapOffset },
        m_pAllocator{ &allocator },
        m_pDescriptorHeap{ heap },
        m_NumHandles{ nHandles },
        m_AllocationManagerId{ allocationManagerId }
    {
        assert(m_pAllocator != nullptr && m_pDescriptorHeap != nullptr);
        auto descriptorSize = m_pAllocator->GetDescriptorSize();

        assert(descriptorSize < std::numeric_limits<uint16>::max());
        m_DescriptorSize = static_cast<uint16>(descriptorSize);
    }

    DescriptorHeapAllocation::DescriptorHeapAllocation(DescriptorHeapAllocation&& allocation) noexcept :
        m_FirstCpuHandle{ std::move(allocation.m_FirstCpuHandle) },
        m_FirstGpuHandle{ std::move(allocation.m_FirstGpuHandle) },
        m_HeapOffset{ std::move(allocation.m_HeapOffset) },
        m_pAllocator{ std::move(allocation.m_pAllocator) },
        m_pDescriptorHeap{ std::move(allocation.m_pDescriptorHeap) },
        m_NumHandles{ std::move(allocation.m_NumHandles) },
        m_AllocationManagerId{ std::move(allocation.m_AllocationManagerId) },
        m_DescriptorSize{ std::move(allocation.m_DescriptorSize) }
    {
        allocation.Reset();
    }

    DescriptorHeapAllocation& DescriptorHeapAllocation::operator=(DescriptorHeapAllocation&& allocation) noexcept
    {
        m_FirstCpuHandle = std::move(allocation.m_FirstCpuHandle);
        m_FirstGpuHandle = std::move(allocation.m_FirstGpuHandle);
        m_HeapOffset = std::move(allocation.m_HeapOffset);
        m_NumHandles = std::move(allocation.m_NumHandles);
        m_pAllocator = std::move(allocation.m_pAllocator);
        m_AllocationManagerId = std::move(allocation.m_AllocationManagerId);
        m_pDescriptorHeap = std::move(allocation.m_pDescriptorHeap);
        m_DescriptorSize = std::move(allocation.m_DescriptorSize);

        allocation.Reset();

        return *this;
    }

    DescriptorHeapAllocation::~DescriptorHeapAllocation()
    {
        if (!IsNull() && m_pAllocator)
            m_pAllocator->SafeFree(std::move(*this), ~QueueMask{0});
    }

    void DescriptorHeapAllocation::Reset()
    {
        m_FirstCpuHandle.ptr = 0;
        m_FirstGpuHandle.ptr = 0;
        m_HeapOffset = 0;
        m_pAllocator = nullptr;
        m_pDescriptorHeap = nullptr;
        m_NumHandles = 0;
        m_AllocationManagerId = InvalidAllocationMgrId;
        m_DescriptorSize = 0;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapAllocation::GetCpuHandle(uint32 offset) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = m_FirstCpuHandle;
        
        if (offset != 0)
            CPUHandle.ptr += m_DescriptorSize * offset;
        
        return CPUHandle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapAllocation::GetGpuHandle(uint32 offset) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = m_FirstGpuHandle;

        if (offset != 0)
            GPUHandle.ptr += m_DescriptorSize * offset;
        
        return GPUHandle;
    }
}