#pragma once
#include "framework.h"
#include "RenderDeviceD3D12.h"

#include <unordered_map>

namespace EduEngine
{
	class GRAPHICS_API DynamicUploadBuffer
	{
	private:
		std::unordered_map<uint32, DynamicHeapAllocation> m_DynHeapAllocation;
		std::unordered_map<uint32, DescriptorHeapAllocation> m_SrvDescriptorAllocation;
		std::unordered_map<uint32, DescriptorHeapAllocation> m_UavDescriptorAllocation;

		RenderDeviceD3D12* m_Device;
		QueueMask m_QueueMask;

	public:
		DynamicUploadBuffer(RenderDeviceD3D12* pDevice, QueueMask queueMask) :
			m_Device(pDevice),
			m_QueueMask(queueMask)
		{
		}

		template <class T>
		void LoadData(DeviceContext* context, const T& initialData);

		void DynamicUploadBuffer::CreateSRV(DeviceContext* context, size_t elemCount, size_t byteStride);
		void DynamicUploadBuffer::CreateUAV(DeviceContext* context, size_t elemCount, size_t byteStride);

		DynamicHeapAllocation GetHeapAllocation(DeviceContext* context) const { return m_DynHeapAllocation.at(context->GetContextId()); }
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