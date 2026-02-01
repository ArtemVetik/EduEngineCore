#include "DeferredPBRLightPass.h"

#include <PBRPrepass.h>

namespace EduEngine
{
	DeferredPBRLightPass::DeferredPBRLightPass(RenderDeviceD3D12* device, DeviceContext* context, DXGI_FORMAT rtFormat)
	{
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(device);
		m_LightsBuffer = std::make_shared<DynamicUploadBuffer>(device);
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

		m_TextureIndexesBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);

		buffDesc.Width = sizeof(MaterialData);
		m_MaterialBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);

		RebuildPSO(device, rtFormat, L"NULL");
	}

	void DeferredPBRLightPass::Update(DeviceContext* context, const Camera* camera, Light* lights, uint32 numLights)
	{
		struct PassData
		{
			XMFLOAT4X4 ViewInv;
			XMFLOAT4X4 ProjInv;
			UINT DirectionalLightsCount;
			XMFLOAT3 CamPos;
			UINT PrefilteredMapLods;
			XMFLOAT3 Padding;
		} passData;

		XMMATRIX viewInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetViewMatrix()));
		XMMATRIX projInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetProjectionMatrix()));

		XMStoreFloat4x4(&passData.ViewInv, viewInv);
		XMStoreFloat4x4(&passData.ProjInv, projInv);
		passData.DirectionalLightsCount = numLights;
		passData.CamPos = camera->GetPosition();
		passData.PrefilteredMapLods = PBRPrepass::PREFILTERED_MIP_LEVELS;

		m_PassBuffer->LoadData(context, passData);

		m_LightsBuffer->LoadData(context, lights, numLights * sizeof(Light));
		m_LightsBuffer->CreateSRV(context, numLights, sizeof(Light));
	}

	void DeferredPBRLightPass::Render(DeviceContext* context, TextureD3D12* target)
	{
		context->GetCommandCtx()->SetRenderTargets(1, &target->GetRTVView()->GetCpuHandle(), false, nullptr);
		m_Pso->CommitAll(context, m_Binder.get());
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
	}

	void DeferredPBRLightPass::RebuildPSO(RenderDeviceD3D12* device, DXGI_FORMAT rtFormat, const wchar_t* debugView)
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

		LPCWSTR macros[]
		{
			debugView, L"1",
			NULL, NULL,
		};

		auto fsQuadVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto lightPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRLightingDeferred.hlsl", L"PS", L"ps_6_6", macros, sDesc);

		D3D12_DEPTH_STENCIL_DESC dssOff = {};
		dssOff.DepthEnable = false;

		DXGI_FORMAT rtFormats[]{ rtFormat };

		m_Pso = std::make_unique<PipelineState>();
		m_Pso->SetDepthStencilState(dssOff);
		m_Pso->SetShader(fsQuadVS);
		m_Pso->SetShader(lightPS);
		m_Pso->SetRTVFormats(1, rtFormats);
		m_Pso->Build(device);

		m_Binder = m_Pso->CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "gLight", m_LightsBuffer);
		m_Binder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbTextureIndexes", m_TextureIndexesBuffer);
		m_Binder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbMaterial", m_MaterialBuffer);
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