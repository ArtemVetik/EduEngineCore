#include "pch.h"
#include "TextureD3D12.h"
#include "DDSTextureLoader.h"

namespace EduEngine
{
	TextureD3D12::TextureD3D12(RenderDeviceD3D12*		  pDevice,
							   const D3D12_RESOURCE_DESC& resourceDesc,
							   const D3D12_CLEAR_VALUE*   clearValue,
							   QueueMask				  queueMask) :
		ResourceViewD3D12(pDevice, queueMask)
	{
		pDevice->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			clearValue,
			IID_PPV_ARGS(m_d3d12Resource.GetAddressOf())
		);
	}

	TextureD3D12::TextureD3D12(RenderDeviceD3D12* pDevice, Microsoft::WRL::ComPtr<ID3D12Resource> resource, QueueMask queueMask) :
		ResourceViewD3D12(pDevice, resource, queueMask)
	{
	}

	TextureD3D12::TextureD3D12(RenderDeviceD3D12* pDevice, DeviceContext* context, std::wstring ddsTexPath, QueueMask queueMask) :
		ResourceViewD3D12(pDevice, queueMask)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> ddsUploadHeap = nullptr;

		HRESULT hr = DirectX::CreateDDSTextureFromFile12(
			m_Device->GetD3D12Device(),
			context->GetCommandCtx()->GetCmdList(),
			ddsTexPath.c_str(),
			m_d3d12Resource,
			ddsUploadHeap
		);

		THROW_IF_FAILED(hr, L"Failed to load dds texture");

		if (ddsUploadHeap)
		{
			ReleaseResourceWrapper releaseResource;
			releaseResource.AddResource(std::move(ddsUploadHeap));

			m_Device->SafeReleaseObject(m_QueueMask, std::move(releaseResource));
		}

		SetState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void TextureD3D12::LoadData(DeviceContext* context, void* dataPtr)
	{
		UINT64 uploadBufferSize = 0;
		m_Device->GetD3D12Device()->GetCopyableFootprints(&m_d3d12Resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

		DynamicHeapAllocation uploadBuff = context->AllocateDynamicSpace(uploadBufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
		
		memcpy(reinterpret_cast<char*>(uploadBuff.GetCpuAddress()), dataPtr, uploadBufferSize);

		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = m_d3d12Resource.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = uploadBuff.GetResource();
		src.PlacedFootprint.Offset = uploadBuff.GetOffset();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		m_Device->GetD3D12Device()->GetCopyableFootprints(&m_d3d12Resource->GetDesc(), 0, 1, uploadBuff.GetOffset(), &src.PlacedFootprint, nullptr, nullptr, nullptr);
		context->GetCommandCtx()->GetCmdList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		SetState(D3D12_RESOURCE_STATE_COPY_DEST);
	}

	void TextureD3D12::LoadData(DeviceContext* context, TextureD3D12* srcTexture, UINT srcSubresource, UINT dstSubresource)
	{
		context->GetCommandCtx()->TransitionResource(srcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
		context->GetCommandCtx()->TransitionResource(this, D3D12_RESOURCE_STATE_COPY_DEST, true);

		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = m_d3d12Resource.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = dstSubresource;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = srcTexture->GetD3D12Resource();
		src.SubresourceIndex = srcSubresource;
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		context->GetCommandCtx()->GetCmdList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	bool TextureD3D12::IsSRGBFormat() const
	{
		if (!m_d3d12Resource)
			return false;

		auto format = m_d3d12Resource->GetDesc().Format;

		return format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
			format == DXGI_FORMAT_BC1_UNORM_SRGB ||
			format == DXGI_FORMAT_BC2_UNORM_SRGB ||
			format == DXGI_FORMAT_BC3_UNORM_SRGB ||
			format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
			format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB ||
			format == DXGI_FORMAT_BC7_UNORM_SRGB;
	}
}