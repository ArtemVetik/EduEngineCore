#include "HZBGenerator.h"

#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	HZBGenerator::HZBGenerator(RenderDeviceD3D12* device) :
		m_Device(device),
		m_Pso(QueueId::Direct)
	{
		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		auto cs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\HZBGenerator.hlsl", L"CS", L"cs_6_6", nullptr, sDesc);

		m_Pso.SetShader(cs);
		m_Pso.Build(m_Device);

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		m_Binder = m_Pso.CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
	}

	void HZBGenerator::Generate(DeviceContext* context, TextureD3D12* depth)
	{
		struct PassData
		{
			XMFLOAT2 InvInputTextureSize;
			UINT MipCount;
			UINT InputTextureIdx;
			UINT OutputTexture0Idx;
			UINT OutputTexture1Idx;
			UINT OutputTexture2Idx;
			UINT OutputTexture3Idx;
		} passData;

		D3D12_RESOURCE_DESC hzbDesc = m_HZBTexture->GetD3D12Resource()->GetDesc();
		D3D12_RESOURCE_DESC depthDesc = depth->GetD3D12Resource()->GetDesc();

		UINT totalMipLevels = hzbDesc.MipLevels;
		XMUINT2 parentTextureSize = XMUINT2(depthDesc.Width, depthDesc.Height);
		UINT outMip = 0;

		while (totalMipLevels > 0)
		{
			UINT mipCount = std::min(totalMipLevels, 4u);

			passData.InvInputTextureSize = XMFLOAT2(2.0f / parentTextureSize.x, 2.0f / parentTextureSize.y);
			passData.MipCount = mipCount;
			
			if (outMip == 0)
				passData.InputTextureIdx = m_DepthSrv.GetGpuHeapIndex();
			else
				passData.InputTextureIdx = m_HZBTexture->GetSRVView()->GetGpuHeapIndex(outMip - 1);

			passData.OutputTexture0Idx = m_HZBTexture->GetUAVView()->GetGpuHeapIndex(outMip + 0);
			passData.OutputTexture1Idx = m_HZBTexture->GetUAVView()->GetGpuHeapIndex(outMip + 1);
			passData.OutputTexture2Idx = m_HZBTexture->GetUAVView()->GetGpuHeapIndex(outMip + 2);
			passData.OutputTexture3Idx = m_HZBTexture->GetUAVView()->GetGpuHeapIndex(outMip + 3);

			m_PassBuffer->LoadData(context, passData);

			m_Pso.CommitAll(context, m_Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch((UINT)std::ceil(parentTextureSize.x / 16.0f), (UINT)std::ceil(parentTextureSize.y / 16.0f), 1);

			totalMipLevels -= mipCount;
			outMip += mipCount;

			parentTextureSize.x >>= mipCount;
			parentTextureSize.y >>= mipCount;
		}
	}

	void HZBGenerator::Resize(TextureD3D12* depth)
	{
		D3D12_RESOURCE_DESC depthDesc = depth->GetD3D12Resource()->GetDesc();

		UINT64 newWidth = floor(depthDesc.Width / 2.0f);
		UINT64 newHeight = floor(depthDesc.Height / 2.0f);

		if (newWidth == 0 || newHeight == 0)
		{
			LOG_ERROR("HZBGenerator::Generate: The new width or height of the HZB texture is 0. ",
				"This can happen if the input depth texture is too small.Width: ", newWidth, ", Height : ", newHeight);
			return;
		}

		DWORD mipCount;
		_BitScanReverse(&mipCount, std::min(newWidth, newHeight));
		mipCount++;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Format = DXGI_FORMAT_R32_FLOAT;
		resDesc.Width = newWidth;
		resDesc.Height = newHeight;
		resDesc.MipLevels = mipCount;
		resDesc.Alignment = 0;
		resDesc.DepthOrArraySize = 1;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = resDesc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = resDesc.Format;
		uavDesc.Texture2D.PlaneSlice = 0;
		uavDesc.Texture2D.MipSlice = 0;

		m_HZBTexture = std::make_unique<TextureD3D12>(m_Device, resDesc, nullptr, QueueId::Direct);
		m_HZBTexture->SetName(L"HZBTexture");
		m_HZBTexture->CreateSRV_Tex2DMip(srvDesc, false);
		m_HZBTexture->CreateUAV_Array(uavDesc, 0, 0, false);

		m_DepthSrv = std::move(m_Device->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1));
		m_Device->GetD3D12Device()->CopyDescriptorsSimple(
			1,
			m_DepthSrv.GetCpuHandle(),
			depth->GetSRVView()->GetCpuHandle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}