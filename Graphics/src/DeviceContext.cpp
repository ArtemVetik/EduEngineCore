#include "DeviceContext.h"
#include "RenderDeviceD3D12.h"

#include <Asserts.h>

namespace EduEngine
{
	DeviceContext::DeviceContext(RenderDeviceD3D12& device, const DeviceContextDesc& desc) :
		m_Device(device),
		m_Desc(desc),
		m_DynamicHeap(device.GetDynamicHeapManager()),
		m_DynamicSuballocationMgr
		{
			{ device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), 2048, "CBV_SRV_UAV_DynSuballocationMgr"},
			{ device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER), 2048, "SAMPLER_DynSuballocationMgr" }
		}
	{
		if (!m_Desc.IsDeferred)
		{
			VERIFY_EXPR(QueueIdIsSupported(m_Desc.Queue), "QueueId \"", m_Desc.Queue, "\" is not supported!");
			m_CmdCtx = device.GetCommandContextPool().AllocateContext(m_Desc.Queue);
		}

		m_ContextId = device.AllocateContextId();
	}

	DeviceContext::~DeviceContext()
	{
		m_Device.FreeContextId(m_ContextId);
	}

	DynamicHeapAllocation DeviceContext::AllocateDynamicSpace(uint64 sizeInBytes, uint64 alignment)
	{
		return m_DynamicHeap.Allocate(sizeInBytes, alignment);
	}

	DescriptorHeapAllocation DeviceContext::AllocateDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count)
	{
		VERIFY_EXPR(type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "");
		return m_DynamicSuballocationMgr[type].Allocate(count);
	}

	void DeviceContext::BeginDeferredFrame(QueueId queueId)
	{
		VERIFY_EXPR(m_Desc.IsDeferred, "\"BeginDeferredFrame()\" supports only for deferred contexts!");
		VERIFY_EXPR(m_CmdCtx == nullptr, "You must free current context before calling \"BeginDeferredFrame()\"");
		m_Desc.Queue = queueId;
		m_CmdCtx = m_Device.GetCommandContextPool().AllocateContext(m_Desc.Queue);
	}

	void DeviceContext::FinishFrame()
	{
		for (size_t i = 0; i < 2; i++)
			m_DynamicSuballocationMgr[i].DiscardAllocations(m_Desc.Queue);

		m_DynamicHeap.ReleasePages(m_Desc.Queue);

		if (m_Desc.IsDeferred)
		{
			m_Device.GetCommandContextPool().FreeContext(std::move(m_CmdCtx));
		}
		else
		{
			m_CmdCtx->Reset();
		}
	}

	CommandContext* DeviceContext::GetCommandCtx()
	{
		VERIFY_EXPR(m_CmdCtx != nullptr, "");
		return m_CmdCtx.get();
	}
}