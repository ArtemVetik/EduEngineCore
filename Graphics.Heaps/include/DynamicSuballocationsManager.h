#pragma once
#include "framework.h"
#include "GPUDescriptorHeap.h"

#include <QueueMask.h>
#include <vector>

namespace EduEngine
{
	class GRAPHICS_HEAPS_API DynamicSuballocationsManager : public IDescriptorAllocator
	{
	public:
		DynamicSuballocationsManager(GPUDescriptorHeap& parentGPUHeap,
									 uint32				dynamicChunkSize,
									 char*				managerName);

		DynamicSuballocationsManager(const DynamicSuballocationsManager&) = delete;
		DynamicSuballocationsManager(DynamicSuballocationsManager&&) = delete;
		DynamicSuballocationsManager& operator = (const DynamicSuballocationsManager&) = delete;
		DynamicSuballocationsManager& operator = (DynamicSuballocationsManager&&) = delete;

		~DynamicSuballocationsManager();

		void DiscardAllocations(QueueMask queueMask);

		virtual DescriptorHeapAllocation Allocate(uint32 count) override;
		virtual void SafeFree(DescriptorHeapAllocation&& allocation, QueueMask queueMask) override;
		virtual uint32 GetDescriptorSize() const override { return m_ParentGPUHeap.GetDescriptorSize(); }
		virtual void FreeAllocation(DescriptorHeapAllocation&& allocation) override { }

	private:
		GPUDescriptorHeap& m_ParentGPUHeap;
		const char*        m_ManagerName;

		// List of chunks allocated from the master GPU descriptor heap. All chunks are disposed at the end
		// of the frame
		std::vector<DescriptorHeapAllocation> m_Suballocations;
		uint32 m_CurrentSuballocationOffset = 0;

		uint32 m_DynamicChunkSize = 0;
	};
}