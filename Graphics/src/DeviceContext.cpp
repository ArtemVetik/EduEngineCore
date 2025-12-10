#include "DeviceContext.h"
#include "RenderDeviceD3D12.h"

#include <Asserts.h>

namespace EduEngine
{
	DeviceContext::DeviceContext(RenderDeviceD3D12& device, D3D12_COMMAND_LIST_TYPE type) :
		m_Device(device),
		m_DynamicHeap(device.GetDynamicHeapManager()),
		m_DynamicSuballocationMgr
		{
			{ device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), 2048, "CBV_SRV_UAV_DynSuballocationMgr"},
			{ device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER), 2048, "SAMPLER_DynSuballocationMgr" }
		}
	{
		VERIFY_EXPR(type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE, "");

		m_CmdCtx = new CommandContext(device, type);
		m_ContextId = device.GetAvailableContextId();
	}

	DeviceContext::~DeviceContext()
	{
		delete m_CmdCtx;
	}

	DynamicHeapAllocation DeviceContext::AllocateDynamicSpace(uint64 sizeInBytes, uint64 alignment)
	{
		return m_DynamicHeap.Allocate(sizeInBytes, alignment);
	}

	DescriptorHeapAllocation DeviceContext::AllocateDynamicDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count)
	{
		VERIFY_EXPR(type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "");
		return m_DynamicSuballocationMgr[type].Allocate(queueMask, count);
	}

	void DeviceContext::FinishFrame()
	{
		QueueMask queueMask = m_CmdCtx->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT ? QueueId::Direct : QueueId::Compute;

		for (size_t i = 0; i < 2; i++)
			m_DynamicSuballocationMgr[i].DiscardAllocations();

		m_DynamicHeap.ReleasePages(queueMask);
	}
}