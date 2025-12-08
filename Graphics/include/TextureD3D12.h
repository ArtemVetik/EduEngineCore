#pragma once
#include "framework.h"
#include "ResourceViewD3D12.h"
#include "RenderDeviceD3D12.h"
#include "DeviceContext.h"

#include <DescriptorHeapAllocation.h>

namespace EduEngine
{
	class GRAPHICS_API TextureD3D12 : public ResourceViewD3D12
	{
	public:
		TextureD3D12(RenderDeviceD3D12*		    pDevice,
					 const D3D12_RESOURCE_DESC& resourceDesc,
					 const D3D12_CLEAR_VALUE*	clearValue,
					 QueueMask					queueMask);

		TextureD3D12(RenderDeviceD3D12* pDevice, Microsoft::WRL::ComPtr<ID3D12Resource> resource, QueueMask queueMask);
		TextureD3D12(RenderDeviceD3D12* pDevice, DeviceContext* context, std::wstring ddsTexPath, QueueMask queueMask);

		void LoadData(DeviceContext* context, void* dataPtr);
		void LoadData(DeviceContext* context, TextureD3D12* srcTexture, UINT srcSubresource = 0, UINT dstSubresource = 0);

		bool IsSRGBFormat() const;
	};
}