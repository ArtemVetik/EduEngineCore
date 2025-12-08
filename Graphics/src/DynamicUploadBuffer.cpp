#include "pch.h"
#include "DynamicUploadBuffer.h"

namespace EduEngine
{
	void DynamicUploadBuffer::CreateSRV(DeviceContext* context, size_t elemCount, size_t byteStride)
	{
		VERIFY_EXPR(m_DynHeapAllocation[context->GetContextId()].GetOffset() % byteStride == 0, "DynamicHeapAllocation offset must be a multiple of byteStride");

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = m_DynHeapAllocation[context->GetContextId()].GetOffset() / byteStride;
		srvDesc.Buffer.NumElements = elemCount;
		srvDesc.Buffer.StructureByteStride = byteStride;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (m_SrvDescriptorAllocation[context->GetContextId()].IsNull())
			m_SrvDescriptorAllocation[context->GetContextId()] = m_Device->AllocateCPUDescriptor(m_QueueMask, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

		m_Device->GetD3D12Device()->CreateShaderResourceView(
			m_DynHeapAllocation[context->GetContextId()].GetResource(),
			&srvDesc,
			m_SrvDescriptorAllocation[context->GetContextId()].GetCpuHandle()
		);
	}

	void DynamicUploadBuffer::CreateUAV(DeviceContext* context, size_t elemCount, size_t byteStride)
	{
		VERIFY_EXPR(m_DynHeapAllocation[context->GetContextId()].GetOffset() % byteStride == 0, "DynamicHeapAllocation offset must be a multiple of byteStride");

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = m_DynHeapAllocation[context->GetContextId()].GetOffset() / byteStride;
		uavDesc.Buffer.NumElements = elemCount;
		uavDesc.Buffer.StructureByteStride = byteStride;

		if (m_UavDescriptorAllocation[context->GetContextId()].IsNull())
			m_UavDescriptorAllocation[context->GetContextId()] = m_Device->AllocateCPUDescriptor(m_QueueMask, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

		m_Device->GetD3D12Device()->CreateUnorderedAccessView(
			m_DynHeapAllocation[context->GetContextId()].GetResource(),
			nullptr,
			&uavDesc,
			m_UavDescriptorAllocation[context->GetContextId()].GetCpuHandle()
		);
	}
}