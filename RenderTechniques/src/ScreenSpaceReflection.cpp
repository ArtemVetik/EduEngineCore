#include "ScreenSpaceReflection.h"

#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	ScreenSpaceReflection::ScreenSpaceReflection(RenderDeviceD3D12* device, DeviceContext* context)
	{
		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		auto cs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ScreenSpaceReflection.hlsl", L"CS", L"cs_6_6", nullptr, sDesc);

		m_Pso = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_Pso->SetShader(cs);
		m_Pso->Build(device);

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(device);

		m_Binder = m_Pso->CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
	}

	void ScreenSpaceReflection::Render(DeviceContext* context, const Camera* camera)
	{
		struct PassData
		{
			XMFLOAT4X4 View;
			XMFLOAT4X4 Proj;
			XMFLOAT4X4 InvProj;
			XMUINT2 ScreenSize;
			UINT AlbedoTexIdx;
			UINT NormalTexIdx;
			UINT MaskTexIdx;
			UINT DepthTexIdx;
			UINT OutTexIdx;
			UINT MaxIterations;
			float DepthThickness;
		} passData;

		XMStoreFloat4x4(&passData.View, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetViewMatrix())));
		XMStoreFloat4x4(&passData.Proj, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetProjectionMatrix())));
		XMStoreFloat4x4(&passData.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetProjectionMatrix()))));
		passData.ScreenSize = camera->GetViewportSize();
		passData.AlbedoTexIdx = m_TexIndexes.AlbedoTexIdx;
		passData.NormalTexIdx = m_TexIndexes.NormalTexIdx;
		passData.MaskTexIdx = m_TexIndexes.MaskTexIdx;
		passData.DepthTexIdx = m_TexIndexes.DepthTexIdx;
		passData.OutTexIdx = m_TexIndexes.OutTexIdx;
		passData.MaxIterations = m_Settings.MaxIterations;
		passData.DepthThickness = m_Settings.DepthThickness;

		m_PassBuffer->LoadData(context, passData);

		m_Pso->CommitAll(context, m_Binder.get());
		context->GetCommandCtx()->GetCmdList()->Dispatch(ceilf(passData.ScreenSize.x / 32.0f), ceilf(passData.ScreenSize.y / 32.0f), 1);
	}
}