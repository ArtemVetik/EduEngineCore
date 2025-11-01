#pragma once
#include "framework.h"
#include "ResourceViewD3D12.h"
#include "RenderDeviceD3D12.h"
#include "DeviceContext.h"

#include "../../Graphics.Heaps/include/DescriptorHeapAllocation.h"

namespace EduEngine
{
	class GRAPHICS_API TextureD3D12 : public ResourceViewD3D12
	{
	public:
		TextureD3D12(RenderDeviceD3D12*		    pDevice,
					 const D3D12_RESOURCE_DESC& resourceDesc,
					 const D3D12_CLEAR_VALUE*	clearValue,
					 QueueID					queueId);

		TextureD3D12(RenderDeviceD3D12* pDevice, Microsoft::WRL::ComPtr<ID3D12Resource> resource, QueueID queueId);
		TextureD3D12(RenderDeviceD3D12* pDevice, DeviceContext* context, std::wstring ddsTexPath, QueueID queueId);
		~TextureD3D12();

		void LoadData(DeviceContext* context, void* dataPtr);

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_DDSuploadHeap;
	};
}