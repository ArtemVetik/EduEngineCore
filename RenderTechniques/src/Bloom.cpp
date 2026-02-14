#include "Bloom.h"

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
	{
		XMFLOAT3 Tint;
		float Threshold;
		float Intensity;
		float Scatter;
	};

	Bloom::Bloom(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height) :
		m_Device(device)
	{
		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto psThreshold = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Bloom.hlsl", L"PSThreshold", L"ps_6_6", nullptr, sDesc);
		auto psBlurH = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Bloom.hlsl", L"PSBlurH", L"ps_6_6", nullptr, sDesc);
		auto psBlurV = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Bloom.hlsl", L"PSBlurV", L"ps_6_6", nullptr, sDesc);
		auto psUpscale = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Bloom.hlsl", L"PSUpscale", L"ps_6_6", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;

		auto rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rast.DepthClipEnable = false;

		m_ThresholdPso.SetDepthStencilState(dss);
		m_ThresholdPso.SetRasterizerState(rast);
		m_ThresholdPso.SetShader(vs);
		m_ThresholdPso.SetShader(psThreshold);
		m_ThresholdPso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_ThresholdPso.Build(m_Device);

		m_HPso.SetDepthStencilState(dss);
		m_HPso.SetRasterizerState(rast);
		m_HPso.SetShader(vs);
		m_HPso.SetShader(psBlurH);
		m_HPso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_HPso.Build(m_Device);

		m_VPso.SetDepthStencilState(dss);
		m_VPso.SetRasterizerState(rast);
		m_VPso.SetShader(vs);
		m_VPso.SetShader(psBlurV);
		m_VPso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_VPso.Build(m_Device);

		m_UpscalePso.SetDepthStencilState(dss);
		m_UpscalePso.SetRasterizerState(rast);
		m_UpscalePso.SetShader(vs);
		m_UpscalePso.SetShader(psUpscale);
		m_UpscalePso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_UpscalePso.Build(m_Device);

		m_ThresholdBinder = m_ThresholdPso.CreateShaderBinder();
		m_HBinder = m_HPso.CreateShaderBinder();
		m_VBinder = m_VPso.CreateShaderBinder();
		m_UpscaleBinder = m_UpscalePso.CreateShaderBinder();

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

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		m_ThresholdBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ConstantsBuffer);
		m_ThresholdBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);

		m_HBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ConstantsBuffer);
		m_HBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		
		m_VBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ConstantsBuffer);
		m_VBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		
		m_UpscaleBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ConstantsBuffer);
		m_UpscaleBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);

		UpdateConstantsBuffer(context);
		Resize(context, width, height);
	}

	void Bloom::Render(DeviceContext* context, UINT inputTexIdx)
	{
		auto* commandContext = context->GetCommandCtx();

		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0, 0, 0, 0 };

		struct PassData
		{
			UINT BloomTex0Idx;
			UINT BloomTex1Idx;
		} passData;

		// bloom threshold pass
		{
			commandContext->TransitionResource(m_BloomMipDown[0].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			commandContext->SetRenderTargets(1, &(m_BloomMipDown[0]->GetRTVView()->GetCpuHandle()), false, nullptr);

			auto desc = m_BloomMipDown[0]->GetD3D12Resource()->GetDesc();
			viewport.Width = desc.Width;
			viewport.Height = desc.Height;
			scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext->SetViewports(&viewport, 1);
			commandContext->SetScissorRects(&scissorRect, 1);

			passData.BloomTex0Idx = inputTexIdx;
			passData.BloomTex1Idx = -1;
			m_PassBuffer->LoadData(context, passData);

			m_ThresholdPso.CommitAll(context, m_ThresholdBinder.get());

			commandContext->GetCmdList()->DrawInstanced(3, 1, 0, 0);
			commandContext->TransitionResource(m_BloomMipDown[0].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		for (int i = 0; i < m_MipNum - 1; i++)
		{
			auto desc = m_BloomMipUp[i + 1]->GetD3D12Resource()->GetDesc();
			viewport.Width = desc.Width;
			viewport.Height = desc.Height;
			scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext->SetViewports(&viewport, 1);
			commandContext->SetScissorRects(&scissorRect, 1);

			// bloom H pass
			{
				commandContext->TransitionResource(m_BloomMipUp[i + 1].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
				commandContext->SetRenderTargets(1, &(m_BloomMipUp[i + 1]->GetRTVView()->GetCpuHandle()), false, nullptr);

				passData.BloomTex1Idx = m_BloomMipDown[i]->GetSRVView()->GetGpuHeapIndex();
				m_PassBuffer->LoadData(context, passData);

				m_HPso.CommitAll(context, m_HBinder.get());

				commandContext->GetCmdList()->DrawInstanced(3, 1, 0, 0);
				commandContext->TransitionResource(m_BloomMipUp[i + 1].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
			}

			// bloom V pass
			{
				commandContext->TransitionResource(m_BloomMipDown[i + 1].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
				commandContext->SetRenderTargets(1, &(m_BloomMipDown[i + 1]->GetRTVView()->GetCpuHandle()), false, nullptr);

				passData.BloomTex1Idx = m_BloomMipUp[i + 1]->GetSRVView()->GetGpuHeapIndex();
				m_PassBuffer->LoadData(context, passData);

				m_VPso.CommitAll(context, m_VBinder.get());

				commandContext->GetCmdList()->DrawInstanced(3, 1, 0, 0);
				commandContext->TransitionResource(m_BloomMipDown[i + 1].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
			}
		}

		// bloom upscaling pass
		for (int i = m_MipNum - 2; i >= 0; i--)
		{
			auto desc = m_BloomMipUp[i]->GetD3D12Resource()->GetDesc();
			viewport.Width = desc.Width;
			viewport.Height = desc.Height;
			scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext->SetViewports(&viewport, 1);
			commandContext->SetScissorRects(&scissorRect, 1);

			commandContext->TransitionResource(m_BloomMipUp[i].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			commandContext->SetRenderTargets(1, &(m_BloomMipUp[i]->GetRTVView()->GetCpuHandle()), false, nullptr);

			passData.BloomTex0Idx = m_BloomMipDown[i]->GetSRVView()->GetGpuHeapIndex();

			if (i == BloomMipCount - 2)
				passData.BloomTex1Idx = m_BloomMipDown[i + 1]->GetSRVView()->GetGpuHeapIndex();
			else
				passData.BloomTex1Idx = m_BloomMipUp[i + 1]->GetSRVView()->GetGpuHeapIndex();

			m_PassBuffer->LoadData(context, passData);

			m_UpscalePso.CommitAll(context, m_UpscaleBinder.get());

			commandContext->GetCmdList()->DrawInstanced(3, 1, 0, 0);
			commandContext->TransitionResource(m_BloomMipUp[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}
	}

	void Bloom::Resize(DeviceContext* context, UINT width, UINT height)
	{
		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		for (size_t i = 0; i < BloomMipCount; i++)
		{
			m_BloomMipDown[i].reset();
			m_BloomMipUp[i].reset();
		}

		D3D12_RESOURCE_DESC resourceDesc;
		ZeroMemory(&resourceDesc, sizeof(resourceDesc));
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.MipLevels = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		D3D12_CLEAR_VALUE clearVal;
		clearVal.Color[0] = 0;
		clearVal.Color[1] = 0;
		clearVal.Color[2] = 0;
		clearVal.Color[3] = 1;
		clearVal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.Texture2D.MipLevels = 1;
		descSRV.Texture2D.MostDetailedMip = 0;
		descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		descSRV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		auto* cmdCtx = context->GetCommandCtx();

		m_MipNum = 0;
		for (int i = 0; i < BloomMipCount; i++)
		{
			resourceDesc.Width = m_Width / (1 << (i + 1));
			resourceDesc.Height = m_Height / (1 << (i + 1));

			if (resourceDesc.Width < 4 || resourceDesc.Height < 4)
				break;

			m_BloomMipDown[i] = std::make_unique<TextureD3D12>(m_Device, resourceDesc, &clearVal, QueueId::Direct);
			m_BloomMipUp[i] = std::make_unique<TextureD3D12>(m_Device, resourceDesc, &clearVal, QueueId::Direct);

			m_BloomMipDown[i]->CreateRTV(nullptr);
			m_BloomMipDown[i]->CreateSRV(&descSRV, false);

			m_BloomMipUp[i]->CreateRTV(nullptr);
			m_BloomMipUp[i]->CreateSRV(&descSRV, false);

			wchar_t bufferName[64];
			swprintf(bufferName, 64, L"BloomMipDown%d_%dx%d", i, (int)resourceDesc.Width, (int)resourceDesc.Height);
			m_BloomMipDown[i]->SetName(bufferName);

			swprintf(bufferName, 64, L"BloomMipUp%d_%dx%d", i, (int)resourceDesc.Width, (int)resourceDesc.Height);
			m_BloomMipUp[i]->SetName(bufferName);

			cmdCtx->TransitionResource(m_BloomMipDown[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
			cmdCtx->TransitionResource(m_BloomMipUp[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);

			m_MipNum++;
		}
	}

	void Bloom::UpdateSettings(DeviceContext* context, Settings settings)
	{
		m_Settings = settings;
		UpdateConstantsBuffer(context);
	}

	void Bloom::UpdateConstantsBuffer(DeviceContext* context)
	{
		ConstantsData data = {};
		data.Tint = m_Settings.Tint;
		data.Threshold = m_Settings.Threshold;
		data.Scatter = m_Settings.Scatter;
		data.Intensity = m_Settings.Intensity;

		m_ConstantsBuffer->LoadData(context, &data);
	}
}