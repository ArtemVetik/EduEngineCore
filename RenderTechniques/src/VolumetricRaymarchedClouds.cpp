#include "VolumetricRaymarchedClouds.h"

#include <SimpleMath.h>
#include <GenerateMipMaps.h>

namespace EduEngine
{
	VolumetricRaymarchedClouds::VolumetricRaymarchedClouds(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height) :
		m_Device(device),
		m_Frame(0),
		m_MipMapGen(device)
	{
		Resize(width, height);

		ShaderResourceDesc resDesc[] =
		{
			{ "gNoise", SHADER_RESOURCE_TYPE_MUTABLE },
			{ "gBlueNoise", SHADER_RESOURCE_TYPE_MUTABLE },
			{ "gScene", SHADER_RESOURCE_TYPE_MUTABLE },
		};

		ShaderDesc sDesc = {};
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;

		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\VolumetricRaymarchedClouds.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);
		auto psBicubicUpscale = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\BicubicUpscale.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;

		m_Pso.SetDepthStencilState(dss);
		m_Pso.SetShader(vs);
		m_Pso.SetShader(ps);
		m_Pso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_Pso.Build(m_Device);

		m_PsoUpscale.SetDepthStencilState(dss);
		m_PsoUpscale.SetShader(vs);
		m_PsoUpscale.SetShader(psBicubicUpscale);
		m_PsoUpscale.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PsoUpscale.Build(m_Device);

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		TextureLoadDesc texLoadDesc = {};
		texLoadDesc.OnCPU = true;
		texLoadDesc.Flags = TextureLoadDesc::Flags::CREATE_SRV;

		m_NoiseTexture = std::make_unique<Texture>(m_Device, texLoadDesc);
		m_NoiseTexture->Load(L"assets\\Textures\\noise2.dds", context);

		m_BlueNoiseTexture = std::make_unique<Texture>(m_Device, texLoadDesc);
		m_BlueNoiseTexture->Load(L"assets\\Textures\\blue-noise.dds", context);

		m_Binder = m_Pso.CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		m_Binder->BindResource(EDU_SHADER_TYPE_PIXEL, "gNoise", m_NoiseTexture->GetD3D12Texture());
		m_Binder->BindResource(EDU_SHADER_TYPE_PIXEL, "gBlueNoise", m_BlueNoiseTexture->GetD3D12Texture());

		m_BinderUpscale = m_PsoUpscale.CreateShaderBinder();
		m_BinderUpscale->BindResource(EDU_SHADER_TYPE_PIXEL, "gScene", m_SceneTexture);
	}

	void VolumetricRaymarchedClouds::Render(DeviceContext* context, const SwapChain& swapChain, const Timer& timer)
	{
		struct PassData
		{
			float Time;
			UINT Frame;
			DirectX::XMFLOAT2 Resolution;
		} passData;

		passData.Time = timer.GetTotalTime();
		passData.Frame = m_Frame++;
		passData.Resolution = DirectX::XMFLOAT2(m_Width, m_Height);

		m_PassBuffer->LoadData(context, passData);

		m_Pso.BeginPSOAndCommitResources(context, m_Binder.get());

		static float clear[4] = { 0, 0, 0, 1 };

		D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height), 0.0f, 1.0f);
		D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height));

		context->GetCommandCtx()->TransitionResource(m_SceneTexture.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		context->GetCommandCtx()->SetViewports(&viewport, 1);
		context->GetCommandCtx()->SetScissorRects(&scissorRect, 1);
		context->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_SceneTexture->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		context->GetCommandCtx()->SetRenderTargets(1, &m_SceneTexture->GetRTVView()->GetCpuHandle(), false, nullptr);
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
		context->GetCommandCtx()->TransitionResource(m_SceneTexture.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		// TODO: optimize for generate each frame
		m_MipMapGen.Generate(context, m_SceneTexture);

		viewport.Width *= m_DownScaleFactor;
		viewport.Height *= m_DownScaleFactor;
		scissorRect.right = static_cast<LONG>(scissorRect.right * m_DownScaleFactor);
		scissorRect.bottom = static_cast<LONG>(scissorRect.bottom * m_DownScaleFactor);

		context->GetCommandCtx()->SetViewports(&viewport, 1);
		context->GetCommandCtx()->SetScissorRects(&scissorRect, 1);
		context->GetCommandCtx()->SetRenderTargets(1, &swapChain.CurrentBackBufferView(), true, &swapChain.DepthStencilView());

		m_PsoUpscale.BeginPSOAndCommitResources(context, m_BinderUpscale.get());
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
	}

	void VolumetricRaymarchedClouds::Resize(UINT width, UINT height)
	{
		m_Width = width / m_DownScaleFactor;
		m_Height = height / m_DownScaleFactor;

		DWORD mipLevels;
		_BitScanReverse(&mipLevels, std::min(m_Width, m_Height));

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = m_Width;
		texDesc.Height = m_Height;
		texDesc.Alignment = 0;
		texDesc.MipLevels = mipLevels + 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = texDesc.Format;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = texDesc.Format;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		D3D12_CLEAR_VALUE clearVal = {};
		clearVal.Color[0] = 0;
		clearVal.Color[1] = 0;
		clearVal.Color[2] = 0;
		clearVal.Color[3] = 1;
		clearVal.Format = texDesc.Format;

		m_SceneTexture = std::make_shared<TextureD3D12>(m_Device, texDesc, &clearVal, QueueId::Direct);
		m_SceneTexture->CreateSRV(&srvDesc, true);
		m_SceneTexture->CreateUAV_Array(uavDesc);
		m_SceneTexture->CreateRTV(&rtvDesc);

		if (m_BinderUpscale)
			m_BinderUpscale->BindResource(EDU_SHADER_TYPE_PIXEL, "gScene", m_SceneTexture);
	}
}