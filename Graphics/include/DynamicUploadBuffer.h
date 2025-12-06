#pragma once
#include "framework.h"
#include "RenderDeviceD3D12.h"

namespace EduEngine
{
	class GRAPHICS_API DynamicUploadBuffer
	{
	private:
		DynamicHeapAllocation m_DynHeapAllocation[RenderDeviceD3D12::MaxDeviceContexts];
		DescriptorHeapAllocation m_SrvDescriptorAllocation[RenderDeviceD3D12::MaxDeviceContexts];
		DescriptorHeapAllocation m_UavDescriptorAllocation[RenderDeviceD3D12::MaxDeviceContexts];

		RenderDeviceD3D12* m_Device;
		QueueID m_QueueId;

	public:
		DynamicUploadBuffer(RenderDeviceD3D12* pDevice, QueueID queueId) :
			m_Device(pDevice),
			m_QueueId(queueId)
		{
		}

		template <class T>
		void LoadData(DeviceContext* context, const T& initialData);

		void DynamicUploadBuffer::CreateSRV(DeviceContext* context, size_t elemCount, size_t byteStride);
		void DynamicUploadBuffer::CreateUAV(DeviceContext* context, size_t elemCount, size_t byteStride);

		DynamicHeapAllocation GetHeapAllocation(DeviceContext* context) const { return m_DynHeapAllocation[context->GetContextId()]; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRVDescriptorCPUHandle(DeviceContext* context) { return m_SrvDescriptorAllocation[context->GetContextId()].GetCpuHandle(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAVDescriptorCPUHandle(DeviceContext* context) { return m_UavDescriptorAllocation[context->GetContextId()].GetCpuHandle(); }
	};

	template<class T>
	inline void DynamicUploadBuffer::LoadData(DeviceContext* context, const T& initialData)
	{
		m_DynHeapAllocation[context->GetContextId()] = context->AllocateDynamicSpace(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		memcpy(m_DynHeapAllocation[context->GetContextId()].GetCpuAddress(), &initialData, sizeof(T));
	}
}