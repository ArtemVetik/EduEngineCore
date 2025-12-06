#pragma once
#include "framework.h"
#include "CommandContext.h"
#include "QueueID.h"

#include <DynamicSuballocationsManager.h>
#include <DynamicHeap.h>

namespace EduEngine
{
	class GRAPHICS_API DeviceContext
	{
	public:
		DeviceContext(RenderDeviceD3D12& device, D3D12_COMMAND_LIST_TYPE type);
		~DeviceContext();

		DynamicHeapAllocation AllocateDynamicSpace(uint64 sizeInBytes, uint64 alignment);
		DescriptorHeapAllocation AllocateDynamicDescriptor(QueueID queueId, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count);

		void FinishFrame();

		CommandContext* GetCommandCtx() const { return m_CmdCtx; }
		uint64 GetContextId() const { return m_ContextId; }

	private:
		CommandContext* m_CmdCtx;

		DynamicSuballocationsManager m_DynamicSuballocationMgr[2];
		DynamicHeap m_DynamicHeap;

		uint64 m_ContextId;

		RenderDeviceD3D12& m_Device;
	};
}