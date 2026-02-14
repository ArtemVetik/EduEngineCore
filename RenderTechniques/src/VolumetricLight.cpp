#include "VolumetricLight.h"

namespace EduEngine
{
	VolumetricLight::VolumetricLight(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height) :
		m_Device(device)
	{
		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\VolumetricLight.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;

		m_Pso.SetDepthStencilState(dss);
		m_Pso.SetShader(vs);
		m_Pso.SetShader(ps);
		m_Pso.SetRTVFormat(DXGI_FORMAT_R16_FLOAT);
		m_Pso.Build(m_Device);

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		m_Binder = m_Pso.CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);

		Resize(width, height);
	}

	void VolumetricLight::Render(DeviceContext* context, const Camera* camera, CSMRendering* csmRendering, Light* light, UINT depthTexId)
	{
		DirectX::XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMMATRIX toTexCoord = camera->GetViewProjMatrix() * T;
		XMVECTOR lightTexC = XMVector3TransformCoord(XMLoadFloat3(&light->Position), toTexCoord);

		struct PassData
		{
			XMFLOAT4X4 InvView;
			XMFLOAT4X4 InvProj;
			XMFLOAT3 LightDir;
			UINT DepthTexIdx;

			XMFLOAT4X4 CascadeTransform[4];
			XMFLOAT4 CascadeShadowSphere[4];
			XMFLOAT4 CascadeShadowRad2;
			XMUINT4 ShadowMapIdx;
			UINT CascadeCount;
			XMFLOAT3 CameraPos;

			UINT NumSteps;
			float Density;
			float Absorption;
			float Intensity;
		} passData;

		XMStoreFloat4x4(&passData.InvView, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetViewMatrix()))));
		XMStoreFloat4x4(&passData.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetProjectionMatrix()))));
		passData.LightDir = light->Direction;
		passData.DepthTexIdx = depthTexId;
		passData.CascadeCount = csmRendering->GetCascadeCount();
		passData.CameraPos = camera->GetPosition();
		passData.CascadeShadowRad2 = csmRendering->GetCascadeRad2();
		passData.NumSteps = m_Settings.NumSteps;
		passData.Density = m_Settings.Density;
		passData.Absorption = m_Settings.Absorption;
		passData.Intensity = m_Settings.Intensity;

		for (uint32 i = 0; i < csmRendering->GetCascadeCount(); i++)
		{
			XMStoreFloat4x4(&passData.CascadeTransform[i], XMMatrixTranspose(csmRendering->GetCascadeTransform(i)));
			passData.CascadeShadowSphere[i] = csmRendering->GetCascadeBoundingSphere(i);
			*(&passData.ShadowMapIdx.x + i) = csmRendering->GetSrv(i)->GetGpuHeapIndex();
		}

		m_PassBuffer->LoadData(context, passData);

		float clear[4] = { 0, 0, 0, 1 };

		m_Pso.CommitAll(context, m_Binder.get());

		context->GetCommandCtx()->GetCmdList()->OMSetRenderTargets(1, &m_LightTexture->GetRTVView()->GetCpuHandle(), false, nullptr);
		context->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_LightTexture->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
	}

	void VolumetricLight::Resize(UINT width, UINT height)
	{
		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Format = DXGI_FORMAT_R16_FLOAT;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = width;
		texDesc.Height = height;
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

		m_LightTexture = std::make_shared<TextureD3D12>(m_Device, texDesc, &clearVal, QueueId::Direct);
		m_LightTexture->CreateSRV(&srvDesc, false);
		m_LightTexture->CreateRTV(&rtvDesc);
		m_LightTexture->SetName(L"VolumetricLightTexture");
	}
}