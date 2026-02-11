#include "DeferredPBRLightPass.h"

#include <IBLRendering.h>
#include <StringUtils.h>

namespace EduEngine
{
	DeferredPBRLightPass::DeferredPBRLightPass(RenderDeviceD3D12* device, DeviceContext* context, DXGI_FORMAT rtFormat) :
		m_Device(device),
		m_RtFormat(rtFormat)
	{
		m_PsoEntry.Name = "DeferredPBRLight";
		m_PsoEntry.DependentParams = 
		{ 
			RenderFeatureID::UseSSAO,
			RenderFeatureID::UseIBL,
			RenderFeatureID::PackNormalsMethod,
			RenderFeatureID::DebugView
		};
		m_PsoEntry.CurrentKey = m_PsoEntry.MakeKeyFromFeatures(g_RenderFeatures);
		m_PsoEntry.BuildPsoFunc = [this]() { return RebuildPSO(); };
		m_PsoEntry.OnPsoUpdated = [this]() 
			{
				m_Binder = m_PsoEntry.Pso->CreateShaderBinder();
				m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
				m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "gLight", m_LightsBuffer);
				m_Binder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbTextureIndexes", m_TextureIndexesBuffer);
				m_Binder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbMaterial", m_MaterialBuffer);
			};

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		m_LightsBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		m_LightsBuffer->CreateSRV(context, 1, sizeof(Light));
		
		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(BuffersIndexesData);
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Alignment = 0;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		m_TextureIndexesBuffer = std::make_shared<BufferD3D12>(m_Device, context, buffDesc, QueueId::Direct);

		buffDesc.Width = sizeof(MaterialData);
		m_MaterialBuffer = std::make_shared<BufferD3D12>(m_Device, context, buffDesc, QueueId::Direct);

		m_PsoEntry.Initialize();
	}

	void DeferredPBRLightPass::Update(DeviceContext* context, const Camera* camera, Light* lights, uint32 numLights, CSMRendering* csmRendering, ReflectionProbesManager* reflectionProbes)
	{
		struct PassData
		{
			XMFLOAT4X4 ViewInv;
			XMFLOAT4X4 ProjInv;
			UINT DirectionalLightsCount;
			XMFLOAT3 CamPos;
			UINT PrefilteredMapLods;
			UINT CascadeCount;
			UINT ReflectionProbeCount;
			UINT ReflectionProbesBuffIdx;
			XMFLOAT4X4 CascadeTransform[CSMRendering::MAX_CASCADES];
			XMFLOAT4 CascadeShadowSphere[CSMRendering::MAX_CASCADES];
			XMFLOAT4 CascadeShadowRad2;
			XMFLOAT4 CascadeDistance;
		} passData;

		XMMATRIX viewInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetViewMatrix()));
		XMMATRIX projInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetProjectionMatrix()));

		XMStoreFloat4x4(&passData.ViewInv, XMMatrixTranspose(viewInv));
		XMStoreFloat4x4(&passData.ProjInv, XMMatrixTranspose(projInv));
		passData.DirectionalLightsCount = numLights;
		passData.CamPos = camera->GetPosition();
		passData.PrefilteredMapLods = IBLRendering::PREFILTERED_MIP_LEVELS;
		passData.CascadeCount = csmRendering->GetCascadeCount();
		passData.ReflectionProbeCount = reflectionProbes->Count();
		passData.ReflectionProbesBuffIdx = reflectionProbes->GetGPUBuffer()->GetSRVView()->GetGpuHeapIndex();
		passData.CascadeShadowRad2 = csmRendering->GetCascadeRad2();

		for (uint32 i = 0; i < csmRendering->GetCascadeCount(); i++)
		{
			XMStoreFloat4x4(&passData.CascadeTransform[i], XMMatrixTranspose(csmRendering->GetCascadeTransform(i)));
			passData.CascadeShadowSphere[i] = csmRendering->GetCascadeBoundingSphere(i);
			*(&passData.CascadeDistance.x + i) = csmRendering->GetCascadeDistance(i);
		}

		m_PassBuffer->LoadData(context, passData);
		m_LightsBuffer->LoadData(context, lights, numLights * sizeof(Light));
		m_LightsBuffer->CreateSRV(context, numLights, sizeof(Light));
	}

	void DeferredPBRLightPass::Render(DeviceContext* context, TextureD3D12* target)
	{
		context->GetCommandCtx()->SetRenderTargets(1, &target->GetRTVView()->GetCpuHandle(), false, nullptr);
		m_PsoEntry.Pso->CommitAll(context, m_Binder.get());
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
	}

	std::shared_ptr<PipelineStateBase> DeferredPBRLightPass::RebuildPSO()
	{
		ShaderResourceDesc sRes[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("gLight", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(sRes);
		sDesc.ResourceDesc = sRes;

		auto debugView = ToWString(std::string(DebugViewsStr[(int)g_RenderFeatures.DebugView]));
		auto packNormal = std::to_wstring((int)g_RenderFeatures.PackNormalsMethod);

		LPCWSTR macrosBuff[]
		{
			debugView.c_str(), L"1",
			L"PACK_NORMALS", packNormal.c_str(),
			NULL, NULL,
		};

		auto fsQuadVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", macrosBuff, sDesc);
		auto lightPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBR_LightingDeferred.hlsl", L"PS", L"ps_6_6", macrosBuff, sDesc);

		D3D12_DEPTH_STENCIL_DESC dssOff = {};
		dssOff.DepthEnable = false;

		DXGI_FORMAT rtFormats[]{ m_RtFormat };

		auto pso = std::make_shared<PipelineState>();
		pso->SetDepthStencilState(dssOff);
		pso->SetShader(fsQuadVS);
		pso->SetShader(lightPS);
		pso->SetRTVFormats(1, rtFormats);
		pso->Build(m_Device);

		return pso;
	}

	void DeferredPBRLightPass::SetBufferIndexes(DeviceContext* context, const BuffersIndexesData& data)
	{
		m_TextureIndexesBuffer->LoadData(context, &data);
	}

	void DeferredPBRLightPass::SetMaterial(DeviceContext* context, const MaterialData& data)
	{
		m_MaterialBuffer->LoadData(context, &data);
	}
}