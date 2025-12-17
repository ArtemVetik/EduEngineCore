#pragma once
#include "framework.h"
#include "CommandContext.h"
#include "QueueMask.h"

#include <DynamicSuballocationsManager.h>
#include <DynamicHeap.h>

namespace EduEngine
{
	struct GRAPHICS_API DeviceContextDesc
	{
		bool IsDeferred;
		QueueId Queue;
	};

	class GRAPHICS_API DeviceContext
	{
	public:
		DeviceContext(RenderDeviceD3D12& device, const DeviceContextDesc& desc);
		~DeviceContext();

		DynamicHeapAllocation AllocateDynamicSpace(uint64 sizeInBytes, uint64 alignment);
		DescriptorHeapAllocation AllocateDynamicDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count);

		void BeginDeferredFrame(QueueId queueId);
		void FinishFrame();

		CommandContext* GetCommandCtx();
		uint64 GetContextId() const { return m_ContextId; }

	private:
		std::unique_ptr<CommandContext> m_CmdCtx = nullptr;

		DynamicSuballocationsManager m_DynamicSuballocationMgr[2];
		DynamicHeap m_DynamicHeap;

		uint64 m_ContextId;

		RenderDeviceD3D12& m_Device;
		DeviceContextDesc m_Desc;
	};
}