#include "SSAO.h"

#include <string>
#include <array>
#include <RandomUtils.h>
#include <DirectXPackedVector.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace EduEngine
{
	SSAO::SSAO(RenderDeviceD3D12* device, DeviceContext* context, uint64 rtWidth, uint64 rtHeight) :
		m_Device(device)
	{
		m_SsaoPsoEntry.Name = "SSAO_Gen";
		m_SsaoPsoEntry.DependentParams = { RenderFeatureID::PackNormalsMethod };
		m_SsaoPsoEntry.CurrentKey = m_SsaoPsoEntry.MakeKeyFromFeatures(g_RenderFeatures);
		m_SsaoPsoEntry.BuildPsoFunc = [this]() { return BuildPSO(false); };
		m_SsaoPsoEntry.OnPsoUpdated = [this]() { m_SsaoBinder = m_SsaoPsoEntry.Pso->CreateShaderBinder(); };

		m_BlurPsoEntry.Name = "SSAO_Blur";
		m_BlurPsoEntry.DependentParams = { RenderFeatureID::PackNormalsMethod };
		m_BlurPsoEntry.CurrentKey = m_BlurPsoEntry.MakeKeyFromFeatures(g_RenderFeatures);
		m_BlurPsoEntry.BuildPsoFunc = [this]() { return BuildPSO(true); };
		m_BlurPsoEntry.OnPsoUpdated = [this]() 
			{
				for (uint32 i = 0; i < 2; i++)
					m_BlurBinder[i] = m_BlurPsoEntry.Pso->CreateShaderBinder();
			};

		Resize(rtWidth, rtHeight);
		
		//
		// Generate RandVectorMap
		//
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = 256;
		texDesc.Height = 256;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		std::array<XMCOLOR, 256 * 256> initData;
		for (int i = 0; i < 256; ++i)
		{
			for (int j = 0; j < 256; ++j)
			{
				XMFLOAT3 v(RandF(), RandF(), RandF());
				initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
			}
		}

		m_RandVectorMap = std::make_shared<TextureD3D12>(m_Device, texDesc, nullptr, QueueId::Direct);
		m_RandVectorMap->LoadData(context, initData.data());
		m_RandVectorMap->CreateSRV(&srvDesc);

		//
		// Build offsets
		//
		m_Offsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
		m_Offsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

		m_Offsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
		m_Offsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

		m_Offsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
		m_Offsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

		m_Offsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
		m_Offsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

		m_Offsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
		m_Offsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

		m_Offsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
		m_Offsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

		m_Offsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
		m_Offsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

		for (int i = 0; i < 14; ++i)
		{
			float s = RandF(0.25f, 1.0f);

			XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&m_Offsets[i]));

			XMStoreFloat4(&m_Offsets[i], v);
		}

		m_SsaoBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		m_ConstantsBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		m_SsaoPsoEntry.Initialize();
		m_BlurPsoEntry.Initialize();
	}

	void SSAO::BindResources(std::shared_ptr<TextureD3D12> normalMap, std::shared_ptr<TextureD3D12> depthMap)
	{
		m_SsaoBinder->DryMutableResources();
		m_SsaoBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbSsao", m_SsaoBuffer);
		m_SsaoBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbSsao", m_SsaoBuffer);
		m_SsaoBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gNormalMap", normalMap);
		m_SsaoBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gDepthMap", depthMap);
		m_SsaoBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gRandomVecMap", m_RandVectorMap);

		for (uint32 i = 0; i < 2; i++)
		{
			m_BlurBinder[i]->DryMutableResources();
			m_BlurBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbSsao", m_SsaoBuffer);
			m_BlurBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbSsao", m_SsaoBuffer);
			m_BlurBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ConstantsBuffer);
			m_BlurBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gNormalMap", normalMap);
			m_BlurBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gDepthMap", depthMap);
			m_BlurBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gInputMap", m_SsaoTexture[i]);
		}
	}

	void SSAO::Update(const Camera* camera, DeviceContext* context)
	{
		struct SsaoData
		{
			XMFLOAT4X4 View;
			XMFLOAT4X4 Proj;
			XMFLOAT4X4 InvProj;
			XMFLOAT4X4 ProjTex;
			XMFLOAT4 OffsetVectors[14];
			XMFLOAT4 BlurWeights[3];
			XMFLOAT2 InvRenderTargetSize;
			float OcclusionRadius;
			float OcclusionFadeStart;
			float OcclusionFadeEnd;
			float SurfaceEpsilon;
		};

		SsaoData ssaoData = {};

		XMMATRIX View = XMLoadFloat4x4(&camera->GetViewMatrix());
		XMMATRIX Proj = XMLoadFloat4x4(&camera->GetProjectionMatrix());

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMStoreFloat4x4(&ssaoData.View, XMMatrixTranspose(View));
		XMStoreFloat4x4(&ssaoData.Proj, XMMatrixTranspose(Proj));
		XMStoreFloat4x4(&ssaoData.InvProj, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(Proj), Proj)));
		XMStoreFloat4x4(&ssaoData.ProjTex, XMMatrixTranspose(Proj * T));
		std::copy(&m_Offsets[0], &m_Offsets[14], &ssaoData.OffsetVectors[0]);
		auto blurWeights = CalcGaussWeights(2.5f);
		ssaoData.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
		ssaoData.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
		ssaoData.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);
		ssaoData.InvRenderTargetSize = XMFLOAT2(1.0f / m_Width, 1.0f / m_Height);
		ssaoData.OcclusionRadius = 0.5f;
		ssaoData.OcclusionFadeStart = 0.2f;
		ssaoData.OcclusionFadeEnd = 1.0f;
		ssaoData.SurfaceEpsilon = 0.05f;

		m_SsaoBuffer->LoadData(context, ssaoData);
	}

	void SSAO::Render(DeviceContext* context)
	{
		context->GetCommandCtx()->SetViewports(&m_Viewport, 1);
		context->GetCommandCtx()->SetScissorRects(&m_ScissorRect, 1);

		context->GetCommandCtx()->TransitionResource(m_SsaoTexture[0].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		const float clear[4] = { 0, 0, 0, 1 };

		context->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_SsaoTexture[0]->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		context->GetCommandCtx()->SetRenderTargets(1, &m_SsaoTexture[0]->GetRTVView()->GetCpuHandle(), false, nullptr);
		m_SsaoPsoEntry.Pso->CommitAll(context, m_SsaoBinder.get());
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
		
		bool horizontalBlur = true;
		m_ConstantsBuffer->LoadData(context, horizontalBlur);

		context->GetCommandCtx()->TransitionResource(m_SsaoTexture[1].get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		context->GetCommandCtx()->TransitionResource(m_SsaoTexture[0].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		context->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_SsaoTexture[1]->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		context->GetCommandCtx()->SetRenderTargets(1, &m_SsaoTexture[1]->GetRTVView()->GetCpuHandle(), false, nullptr);
		m_BlurPsoEntry.Pso->CommitAll(context, m_BlurBinder[0].get());
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		horizontalBlur = false;
		m_ConstantsBuffer->LoadData(context, horizontalBlur);

		context->GetCommandCtx()->TransitionResource(m_SsaoTexture[0].get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		context->GetCommandCtx()->TransitionResource(m_SsaoTexture[1].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		context->GetCommandCtx()->SetRenderTargets(1, &m_SsaoTexture[0]->GetRTVView()->GetCpuHandle(), false, nullptr);
		m_BlurPsoEntry.Pso->CommitAll(context, m_BlurBinder[1].get());
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
	}

	void SSAO::Resize(uint64 rtWidth, uint64 rtHeight)
	{
		m_Width = rtWidth / 2;
		m_Height = rtHeight / 2;

		m_Viewport.TopLeftX = 0.0f;
		m_Viewport.TopLeftY = 0.0f;
		m_Viewport.Width = m_Width;
		m_Viewport.Height = m_Height;
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;

		m_ScissorRect = { 0, 0, (int)m_Width, (int)m_Height };

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Format = SSAO_FORMAT;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = m_Width;
		texDesc.Height = m_Height;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

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

		for (uint32 i = 0; i < 2; i++)
		{
			m_SsaoTexture[i] = std::make_shared<TextureD3D12>(m_Device, texDesc, &clearVal, QueueId::Direct);
			m_SsaoTexture[i]->CreateSRV(&srvDesc);
			m_SsaoTexture[i]->CreateRTV(&rtvDesc);

			wchar_t bufferName[16];
			swprintf(bufferName, 16, L"m_SsaoTex-%d", i);
			m_SsaoTexture[i]->SetName(bufferName);
		}
	}

	std::shared_ptr<PipelineState> SSAO::BuildPSO(bool blurPso)
	{
		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			1, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
			0.0f,
			0,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			3, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

		std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
		{
			pointClamp, linearClamp, depthMapSam, linearWrap
		};

		ShaderResourceDesc sRes[]
		{
			ShaderResourceDesc("cbSsao", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbConstants", SHADER_RESOURCE_TYPE_DYNAMIC), // TODO: make mutable
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(sRes);
		sDesc.ResourceDesc = sRes;

		std::wstring packNormals = std::to_wstring((int)g_RenderFeatures.PackNormalsMethod);

		LPCWSTR defines[]
		{
			L"WORLD_SPACE_NORMALS", L"1", // TODO: create parameter
			L"PACK_NORMALS", packNormals.c_str(),
			NULL, NULL
		};

		auto VS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\SSAO.hlsl", L"VS", L"vs_6_0", defines, sDesc);
		std::shared_ptr<ShaderD3D12> PS = nullptr;

		if (blurPso)
			PS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\SSAO.hlsl", L"PS_Blur", L"ps_6_0", defines, sDesc);
		else
			PS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\SSAO.hlsl", L"PS_SSAO", L"ps_6_0", defines, sDesc);
		
		D3D12_DEPTH_STENCIL_DESC dssOff = {};
		dssOff.DepthEnable = false;

		auto pso = std::make_shared<PipelineState>();
		pso->SetDepthStencilState(dssOff);
		pso->SetShader(VS);
		pso->SetShader(PS);
		pso->SetRTVFormat(SSAO_FORMAT);
		pso->Build(m_Device, staticSamplers.data(), staticSamplers.size());

		return pso;

	}

	std::vector<float> SSAO::CalcGaussWeights(float sigma)
	{
		float twoSigma2 = 2.0f * sigma * sigma;

		// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
		// For example, for sigma = 3, the width of the bell curve is 
		int blurRadius = (int)ceil(2.0f * sigma);

		VERIFY_EXPR(blurRadius <= MAX_BLUR_RADIUS, "");

		std::vector<float> weights;
		weights.resize(2 * blurRadius + 1);

		float weightSum = 0.0f;

		for (int i = -blurRadius; i <= blurRadius; ++i)
		{
			float x = (float)i;

			weights[i + blurRadius] = expf(-x * x / twoSigma2);

			weightSum += weights[i + blurRadius];
		}

		// Divide by the sum so all the weights add up to 1.0.
		for (int i = 0; i < weights.size(); ++i)
		{
			weights[i] /= weightSum;
		}

		return weights;
	}
}