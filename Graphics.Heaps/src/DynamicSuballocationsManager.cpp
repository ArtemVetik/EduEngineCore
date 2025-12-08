#include "pch.h"
#include "DynamicSuballocationsManager.h"

namespace EduEngine
{
	DynamicSuballocationsManager::DynamicSuballocationsManager(GPUDescriptorHeap& parentGPUHeap,
															   uint32             dynamicChunkSize,
															   char*			  managerName) :
        m_ParentGPUHeap{ parentGPUHeap },
        m_DynamicChunkSize { dynamicChunkSize },
        m_ManagerName{ std::move(managerName) }
    {
    }

    DynamicSuballocationsManager::~DynamicSuballocationsManager()
    {
        for (size_t i = 0; i < MaxQueueMask; i++)
            VERIFY_EXPR(m_Suballocations[i].empty(), "");
    }

    void DynamicSuballocationsManager::DiscardAllocations()
    {
        for (size_t i = 0; i < MaxQueueMask; i++)
            m_Suballocations[i].clear();
    }

    DescriptorHeapAllocation DynamicSuballocationsManager::Allocate(QueueMask queueMask, uint32 count)
    {
        VERIFY_EXPR(queueMask > 0 && queueMask <= MaxQueueMask, "");

        // Check if there are no chunks or the last chunk does not have enough space
        if (m_Suballocations[queueMask].empty() || m_CurrentSuballocationOffset[queueMask] + count > m_Suballocations[queueMask].back().GetNumHandles())
        {
            // Request new chunk from the GPU descriptor heap
            auto suballocationSize = std::max(m_DynamicChunkSize, count);
            auto newDynamicSubAllocation = m_ParentGPUHeap.AllocateDynamic(queueMask, suballocationSize);
            m_Suballocations[queueMask].emplace_back(std::move(newDynamicSubAllocation));
            m_CurrentSuballocationOffset[queueMask] = 0;
        }

        // Perform suballocation from the last chunk
        auto& currentSuballocation = m_Suballocations[queueMask].back();

        auto managerId = currentSuballocation.GetAllocationManagerId();

        DescriptorHeapAllocation allocation(*this,
            currentSuballocation.GetDescriptorHeap(),
            currentSuballocation.GetCpuHandle(m_CurrentSuballocationOffset[queueMask]),
            currentSuballocation.GetGpuHandle(m_CurrentSuballocationOffset[queueMask]),
            count,
            static_cast<uint16>(managerId),
            queueMask);
        m_CurrentSuballocationOffset[queueMask] += count;

        return allocation;
    }

    void DynamicSuballocationsManager::SafeFree(DescriptorHeapAllocation&& allocation)
    {
        // Do nothing. Dynamic allocations are not disposed individually, but as whole chunks
        // at the end of the frame by ReleaseAllocations()
        allocation.Reset();
    }
}