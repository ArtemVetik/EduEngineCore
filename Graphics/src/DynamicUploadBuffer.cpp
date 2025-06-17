#include "pch.h"
#include "DynamicUploadBuffer.h"

namespace EduEngine
{
	void DynamicUploadBuffer::CreateSRV(size_t elemCount, size_t byteStride)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = m_DynamicAllocation.Offset / byteStride;
		srvDesc.Buffer.NumElements = elemCount;
		srvDesc.Buffer.StructureByteStride = byteStride;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (m_SrvDescriptorAllocation.IsNull())
			m_SrvDescriptorAllocation = m_Device->AllocateCPUDescriptor(m_QueueId, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		
		m_Device->GetD3D12Device()->CreateShaderResourceView(m_DynamicAllocation.pBuffer, &srvDesc, m_SrvDescriptorAllocation.GetCpuHandle());
	}

	void DynamicUploadBuffer::CreateUAV(size_t elemCount, size_t byteStride)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = m_DynamicAllocation.Offset / byteStride;
		uavDesc.Buffer.NumElements = elemCount;
		uavDesc.Buffer.StructureByteStride = byteStride;

		if (m_UavDescriptorAllocation.IsNull())
			m_UavDescriptorAllocation = m_Device->AllocateCPUDescriptor(m_QueueId, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

		m_Device->GetD3D12Device()->CreateUnorderedAccessView(m_DynamicAllocation.pBuffer, nullptr, &uavDesc, m_UavDescriptorAllocation.GetCpuHandle());
	}

	void DynamicUploadBuffer::CreateAllocation(size_t size)
	{
		m_DynamicAllocation = m_Device->AllocateDynamicUploadGPUDescriptor(m_QueueId, size);
	}
}