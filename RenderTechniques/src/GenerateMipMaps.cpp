#include "GenerateMipMaps.h"

#include <Asserts.h>

namespace EduEngine
{
	GenerateMipMaps::GenerateMipMaps(RenderDeviceD3D12* device) :
		m_Device(device),
		m_PSO(QueueID::Direct)
	{
		ShaderDesc desc = {};
		desc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		desc.ResourceNum = 0;

		auto shader = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\GenerateMipsCS.hlsl", L"main", L"cs_6_6", nullptr, desc);

		m_PSO.SetShader(shader);
		m_PSO.Build(device);
		m_PSO.SetName(L"GenerateMipMaps PSO");
	}

	void GenerateMipMaps::Generate(DeviceContext* context, std::shared_ptr<TextureD3D12> texture)
	{
		auto texDesc = texture->GetD3D12Resource()->GetDesc();

		if (texDesc.MipLevels == 1)
			return;

		if (texDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			LOG_ERROR("Mip maps can only be generated for texture with Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D");
			return;
		}

		if (texDesc.SampleDesc.Count > 1)
		{
			LOG_ERROR("Mip maps can only be generated for textures with SampleDesc.Count = 1");
			return;
		}

		D3D12_FEATURE_DATA_FORMAT_SUPPORT fs = {};
		fs.Format = texDesc.Format;
		fs.Support1 = D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW;
		fs.Support2 = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;

		if (FAILED(m_Device->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof(fs))))
		{
			LOG_ERROR("Texture format is not supported for UAV Load/Store");
			return;
		}

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(CB);
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		std::shared_ptr<BufferD3D12> cbRes = std::make_shared<BufferD3D12>(m_Device, context, buffDesc, QueueID::Direct);
		m_PSO.GetShaderBinder()->BindResource(EDU_SHADER_TYPE_COMPUTE, "CB0", cbRes);

		std::shared_ptr<TextureD3D12> tmpTexture = nullptr;

		if (texDesc.DepthOrArraySize > 1)
		{
			D3D12_RESOURCE_DESC tmpTexDesc = {};
			tmpTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			tmpTexDesc.Alignment = 0;
			tmpTexDesc.Width = texDesc.Width;
			tmpTexDesc.Height = texDesc.Height;
			tmpTexDesc.DepthOrArraySize = 1;
			tmpTexDesc.MipLevels = texDesc.MipLevels;
			tmpTexDesc.Format = texDesc.Format;
			tmpTexDesc.SampleDesc.Count = 1;
			tmpTexDesc.SampleDesc.Quality = 0;
			tmpTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format = texDesc.Format;
			srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Format = texDesc.Format;

			tmpTexture = std::make_shared<TextureD3D12>(m_Device, tmpTexDesc, nullptr, QueueID::Direct);
			tmpTexture->CreateSRV(&srvDesc);
			tmpTexture->CreateUAV_Array(uavDesc);

			// TODO: SRV Subresource -> D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
			//		 UAV Subresource -> D3D12_RESOURCE_STATE_UNORDERED_ACCESS
			context->GetCommandCtx()->TransitionResource(tmpTexture.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);

			for (uint16 depth = 0; depth < texDesc.DepthOrArraySize; depth++)
			{
				tmpTexture->LoadData(context, texture.get(),
					D3D12CalcSubresource(0, depth, 0, texDesc.MipLevels, texDesc.DepthOrArraySize),
					D3D12CalcSubresource(0, 0, 0, texDesc.MipLevels, 1));

				GenerateInternal(context, tmpTexture, cbRes.get());

				for (uint8 mipLevel = 0; mipLevel < texDesc.MipLevels - 1; mipLevel++)
				{
					texture->LoadData(context, tmpTexture.get(),
						D3D12CalcSubresource(mipLevel + 1, 0, 0, texDesc.MipLevels, 1),
						D3D12CalcSubresource(mipLevel + 1, depth, 0, texDesc.MipLevels, texDesc.DepthOrArraySize));
				}
			}
		}
		else
		{
			GenerateInternal(context, texture, cbRes.get());
		}
	}

	void GenerateMipMaps::GenerateInternal(DeviceContext* context, std::shared_ptr<TextureD3D12> texture, BufferD3D12* cbRes)
	{
		auto texDesc = texture->GetD3D12Resource()->GetDesc();

		VERIFY_EXPR(texDesc.DepthOrArraySize == 1, "");

		for (UINT16 srcMip = 0; srcMip < texDesc.MipLevels - 1u; )
		{
			uint64 dstWidth = std::max<uint64>(1, texDesc.Width >> (srcMip + 1));
			uint32 dstHeight = std::max<uint32>(1, texDesc.Height >> (srcMip + 1));

			DWORD mipCount;
			_BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));

			mipCount = std::min<DWORD>(4, mipCount + 1);
			mipCount = std::min(texDesc.MipLevels - srcMip - 1, (int)mipCount);

			CB cbData = {};
			cbData.SrcMipLevel = srcMip;
			cbData.NumMipLevels = mipCount;
			cbData.TexelSize.x = 1.0f / dstWidth;
			cbData.TexelSize.y = 1.0f / dstHeight;
			cbData.IsSRGB = texture->IsSRGBFormat();

			// 0b00(0): Both width and height are even.
			// 0b01(1): Width is odd, height is even.
			// 0b10(2): Width is even, height is odd.
			// 0b11(3): Both width and height are odd.
			cbData.NonPowerTwo = (UINT)((texDesc.Height & 1u) << 1) | (texDesc.Width & 1u);
			cbRes->LoadData(context, &cbData);

			m_PSO.GetShaderBinder()->BindResource(EDU_SHADER_TYPE_COMPUTE, "SrcMip", texture);
			m_PSO.GetShaderBinder()->BindResource(EDU_SHADER_TYPE_COMPUTE, "OutMip1", texture, srcMip + 1);
			m_PSO.GetShaderBinder()->BindResource(EDU_SHADER_TYPE_COMPUTE, "OutMip2", texture, srcMip + 2);
			m_PSO.GetShaderBinder()->BindResource(EDU_SHADER_TYPE_COMPUTE, "OutMip3", texture, srcMip + 3);
			m_PSO.GetShaderBinder()->BindResource(EDU_SHADER_TYPE_COMPUTE, "OutMip4", texture, srcMip + 4);

			m_PSO.CommitAll(context);
			m_PSO.GetShaderBinder()->DryMutableResources();

			context->GetCommandCtx()->GetCmdList()->Dispatch(dstWidth / 8 + 1, dstHeight / 8 + 1, 1);

			srcMip += mipCount;
		}
	}
}