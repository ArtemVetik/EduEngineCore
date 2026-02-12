#include "ScreenSpaceReflection.h"

#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
	{
		XMUINT2 ScreenSize;
		UINT InputColorTexIdx;
		UINT NormalTexIdx;

		UINT MaskTexIdx;
		UINT DepthTexIdx;
		UINT SSRTexIdx0;
		UINT SSRTexIdx1;

		UINT MaxIterations;
		float DepthThickness;
		XMUINT2 Padding;

		XMFLOAT4 BlurWeights[3];
	};

	std::vector<float> CalcGaussWeights(float sigma) // TODO: Duplicated in SSAO.h
	{
		float twoSigma2 = 2.0f * sigma * sigma;

		// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
		// For example, for sigma = 3, the width of the bell curve is 
		int blurRadius = (int)ceil(2.0f * sigma);

		VERIFY_EXPR(blurRadius <= 5, ""); // TODO: magic number

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

	ScreenSpaceReflection::ScreenSpaceReflection(RenderDeviceD3D12* device, DeviceContext* context, UINT rtWidth, UINT rtHeight) :
		m_Device(device)
	{
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		m_BlurPassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.Width = sizeof(ConstantsData);
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		m_ConstantsBuffer = std::make_shared<BufferD3D12>(m_Device, context, buffDesc, QueueId::Direct);

		m_MainPso.Name = "SSR_MainPass";
		m_MainPso.DependentParams = { RenderFeatureID::PackNormalsMethod };
		m_MainPso.BuildPsoFunc = [this]() { return BuildPso(false); };
		m_MainPso.OnPsoUpdated = [this]()
		{
			m_MainBinder = m_MainPso.Pso->CreateShaderBinder();
			m_MainBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
			m_MainBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ConstantsBuffer);
		};
		m_MainPso.Initialize();

		m_BlurPso.Name = "SSR_BlurPass";
		m_BlurPso.DependentParams = { RenderFeatureID::PackNormalsMethod };
		m_BlurPso.BuildPsoFunc = [this]() { return BuildPso(true); };
		m_BlurPso.OnPsoUpdated = [this]()
			{
				m_BlurBinder = m_BlurPso.Pso->CreateShaderBinder();
				m_BlurBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
				m_BlurBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbBlurPass", m_BlurPassBuffer);
				m_BlurBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ConstantsBuffer);
			};
		m_BlurPso.Initialize();

		Resize(rtWidth, rtHeight);
		UpdateConstantsBuffer(context);
	}

	void ScreenSpaceReflection::UpdateIndexes(DeviceContext* context, TextureIndexes texIndexes)
	{
		m_TexIndexes = texIndexes;
		UpdateConstantsBuffer(context);
	}

	void ScreenSpaceReflection::UpdateSettings(DeviceContext* context, Settings settings)
	{
		m_Settings = settings;
		UpdateConstantsBuffer(context);
	}

	void ScreenSpaceReflection::Render(DeviceContext* context, const Camera* camera)
	{
		struct PassData
		{
			XMFLOAT4X4 View;
			XMFLOAT4X4 Proj;
			XMFLOAT4X4 InvProj;
		} passData;

		XMStoreFloat4x4(&passData.View, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetViewMatrix())));
		XMStoreFloat4x4(&passData.Proj, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetProjectionMatrix())));
		XMStoreFloat4x4(&passData.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetProjectionMatrix()))));

		m_PassBuffer->LoadData(context, passData);

		m_MainPso.Pso->CommitAll(context, m_MainBinder.get());
		context->GetCommandCtx()->GetCmdList()->Dispatch(ceilf(m_Viewport.Width / 32.0f), ceilf(m_Viewport.Height / 32.0f), 1);

		if (!m_Settings.BlurEnabled)
			return;

		//
		// Blur pass
		//

		bool horizontal = true;
		m_BlurPassBuffer->LoadData(context, horizontal);

		m_BlurPso.Pso->CommitAll(context, m_BlurBinder.get());
		context->GetCommandCtx()->GetCmdList()->Dispatch(ceilf(camera->GetViewportSize().x / 8.0f), ceilf(camera->GetViewportSize().y / 8.0f), 1);
		
		horizontal = false;
		m_BlurPassBuffer->LoadData(context, horizontal);

		m_BlurPso.Pso->CommitBinder(context, m_BlurBinder.get());
		context->GetCommandCtx()->GetCmdList()->Dispatch(ceilf(camera->GetViewportSize().x / 8.0f), ceilf(camera->GetViewportSize().y / 8.0f), 1);
	}

	void ScreenSpaceReflection::Resize(UINT width, UINT height)
	{
		if (m_Viewport.Width == width && m_Viewport.Height == height)
			return;

		m_Viewport.TopLeftX = 0.0f;
		m_Viewport.TopLeftY = 0.0f;
		m_Viewport.Width = width;
		m_Viewport.Height = height;
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;

		m_ScissorRect = { 0, 0, (int)width, (int)height };

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;

		for (uint32 i = 0; i < 2; i++)
		{
			m_ReflectionTex[i] = std::make_shared<TextureD3D12>(m_Device, texDesc, nullptr, QueueId::Direct);
			m_ReflectionTex[i]->CreateSRV(&srvDesc); // TODO: create GPU handles, not CPU (make bindless)
			m_ReflectionTex[i]->CreateUAV(&uavDesc);

			wchar_t bufferName[16];
			swprintf(bufferName, 16, L"m_SSRTex-%d", i);
			m_ReflectionTex[i]->SetName(bufferName);
		}

		m_GpuHandles = std::move(m_Device->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2));

		m_Device->GetD3D12Device()->CopyDescriptorsSimple(1,
			m_GpuHandles.GetCpuHandle(0),
			m_ReflectionTex[0]->GetSRVView()->GetCpuHandle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		m_Device->GetD3D12Device()->CopyDescriptorsSimple(1,
			m_GpuHandles.GetCpuHandle(1),
			m_ReflectionTex[1]->GetSRVView()->GetCpuHandle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	std::shared_ptr<TextureD3D12> ScreenSpaceReflection::GetSSRTextureShared() const
	{
		if (m_Settings.BlurEnabled)
			return m_ReflectionTex[1];
		else
			return m_ReflectionTex[0];
	}

	std::shared_ptr<PipelineStateBase> ScreenSpaceReflection::BuildPso(bool blurPso)
	{
		auto packNormal = std::to_wstring((int)g_RenderFeatures.PackNormalsMethod);

		ShaderResourceDesc res[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbBlurPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(res);
		sDesc.ResourceDesc = res;

		LPCWSTR macros[]
		{
			L"PACK_NORMALS", packNormal.c_str(),
			NULL, NULL
		};

		const wchar_t* pass = blurPso ? L"CS_Blur" : L"CS_Main";
		auto cs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ScreenSpaceReflection.hlsl", pass, L"cs_6_6", macros, sDesc);

		auto pso = std::make_unique<ComputePipelineState>(QueueId::Direct);
		pso->SetShader(cs);
		pso->Build(m_Device);

		return pso;
	}

	void ScreenSpaceReflection::UpdateConstantsBuffer(DeviceContext* context)
	{
		static auto weights = CalcGaussWeights(2.5f);

		ConstantsData data = {};
		data.ScreenSize = XMUINT2(m_Viewport.Width, m_Viewport.Height);
		data.InputColorTexIdx = m_TexIndexes.ColorTexIdx;
		data.NormalTexIdx = m_TexIndexes.NormalTexIdx;
		data.MaskTexIdx = m_TexIndexes.MaskTexIdx;
		data.DepthTexIdx = m_TexIndexes.DepthTexIdx;
		data.SSRTexIdx0 = m_GpuHandles.GetGpuHeapIndex(0);
		data.SSRTexIdx1 = m_GpuHandles.GetGpuHeapIndex(1);
		data.MaxIterations = m_Settings.MaxIterations;
		data.DepthThickness = m_Settings.DepthThickness;
		data.BlurWeights[0] = XMFLOAT4(&weights[0]);
		data.BlurWeights[1] = XMFLOAT4(&weights[4]);
		data.BlurWeights[2] = XMFLOAT4(&weights[8]);

		m_ConstantsBuffer->LoadData(context, &data);
	}
}