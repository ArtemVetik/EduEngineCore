#pragma once
#include "framework.h"
#include "RenderDeviceD3D12.h"

#include <RawMemoryAllocator.h>
#include <unordered_map>

namespace EduEngine
{
	class GRAPHICS_API DynamicUploadBuffer
	{
	private:
		using DynHeapAlloc = MemoryAllocator::RawMemoryAllocator<std::pair<const uint32, DynamicHeapAllocation>>;
		using DescriptorHeapAlloc = MemoryAllocator::RawMemoryAllocator<std::pair<const uint32, DescriptorHeapAllocation>>;

		std::unordered_map<uint32, DynamicHeapAllocation, std::hash<uint32>, std::equal_to<uint32>, DynHeapAlloc> m_DynHeapAllocation;
		std::unordered_map<uint32, DescriptorHeapAllocation, std::hash<uint32>, std::equal_to<uint32>, DescriptorHeapAlloc> m_SrvDescriptorAllocation;
		std::unordered_map<uint32, DescriptorHeapAllocation, std::hash<uint32>, std::equal_to<uint32>, DescriptorHeapAlloc> m_UavDescriptorAllocation;

		RenderDeviceD3D12* m_Device;

	public:
		DynamicUploadBuffer(RenderDeviceD3D12* pDevice) :
			m_Device(pDevice)
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