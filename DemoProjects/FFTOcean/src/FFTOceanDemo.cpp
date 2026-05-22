#include "FFTOceanDemo.h"

#include <DemoHelpers.h>
#include <d3dx12.h>

#include <algorithm>
#include <cmath>

namespace EduEngine
{
	void FFTOceanDemo::FillMainLightPosFromSunAngles(XMFLOAT3& outMainLightPos) const
	{
		const float elClamped = std::clamp(m_SunElevationDegrees, -89.9f, 89.9f);
		const float azRad = XMConvertToRadians(m_SunAzimuthDegrees);
		const float elRad = XMConvertToRadians(elClamped);
		const float cosEl = cosf(elRad);
		const float kScale = 1000.f;
		outMainLightPos.x = cosEl * cosf(azRad) * kScale;
		outMainLightPos.y = sinf(elRad) * kScale;
		outMainLightPos.z = cosEl * sinf(azRad) * kScale;
	}

	void FFTOceanDemo::RefreshOceanGpuConstants()
	{
		m_ConstantsData.NbCascades = m_OceanInitialSettings.CascadesCount;
		m_ConstantsData.WavelengthsIdx = m_FFTOcean->GetWaveLengthSrvGpuBufferIdx();
		m_ConstantsData.DisplacementsTexturesIdx = m_FFTOcean->GetDisplacementsTexSrvGpuBufferIdx();
		m_ConstantsData.DerivativesTexturesIdx = m_FFTOcean->GetDerivativesTexSrvGpuBufferIdx();
		m_ConstantsData.TurbulenceTexturesIdx = m_FFTOcean->GetTurbulenceTexSrvGpuBufferIdx();
		m_ConstantsData.ReflectionCubeIdx = m_Atmosphere->GetReflectionCube()->GetSRVView()->GetGpuHeapIndex();

		m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
	}

	void FFTOceanDemo::RecreateFFTOceanPreservingSettings()
	{
		FFTOcean::Settings saved = m_FFTOcean->GetSettings();
		m_FFTOcean = std::make_unique<FFTOcean>(GetDevice(), GetMainContext(), m_OceanInitialSettings);
		m_FFTOcean->UpdateSettings(saved);
		RefreshOceanGpuConstants();
	}

	void FFTOceanDemo::OnStartUp()
	{
		GetCamera()->UpdateNearFar(0.1f, 10000.f);

		m_OceanInitialSettings.TextureSize = 512;
		m_OceanInitialSettings.CascadesCount = 3;
		m_OceanInitialSettings.Cascades[0] = { 1530, 1e12f, 1e-10f, 0.4f, 0.1f };
		m_OceanInitialSettings.Cascades[1] = { 1000, 1e7f, 1e-7f, 0.3f, 0.2f };
		m_OceanInitialSettings.Cascades[2] = { 201, 1000000.0f, 1e-5f, 0.1f, 0.1f };

		m_MeshGenerator = std::make_unique<MeshGenerator>(GetDevice(), GetMainContext(), 512, 512, 5000);
		m_FFTOcean = std::make_unique<FFTOcean>(GetDevice(), GetMainContext(), m_OceanInitialSettings);
		m_Atmosphere = std::make_unique<Atmosphere>(GetDevice(), GetMainContext());

		ShaderResourceDesc resDesc[] = 
		{
			{ "cbPass", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto hs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"Hull", L"hs_6_6", nullptr, sDesc);
		auto ds = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"Domain", L"ds_6_6", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		auto vsPostProc = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto psPostProc = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PostProc.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = TRUE;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		D3D12_INPUT_ELEMENT_DESC elementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_LAYOUT_DESC inputLayout = {};
		inputLayout.NumElements = _countof(elementDesc);
		inputLayout.pInputElementDescs = elementDesc;

		m_DrawPSO.SetShader(vs);
		m_DrawPSO.SetShader(ps);
		m_DrawPSO.SetShader(hs);
		m_DrawPSO.SetShader(ds);
		m_DrawPSO.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
		m_DrawPSO.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_DrawPSO.SetDepthStencilState(dss);
		m_DrawPSO.SetInputLayout(inputLayout);
		m_DrawPSO.Build(GetDevice());
		
		dss.DepthEnable = false;

		m_PostProcPSO.SetShader(vsPostProc);
		m_PostProcPSO.SetShader(psPostProc);
		m_PostProcPSO.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PostProcPSO.SetDepthStencilState(dss);
		m_PostProcPSO.Build(GetDevice());

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

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
		m_ConstantsBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, QueueId::Direct);

		RefreshOceanGpuConstants();

		m_DrawBinder = m_DrawPSO.CreateShaderBinder();
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_HULL, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_DOMAIN, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_HULL, "cbConstants", m_ConstantsBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_DOMAIN, "cbConstants", m_ConstantsBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ConstantsBuffer);

		m_PostProcBinder = m_PostProcPSO.CreateShaderBinder();
		m_PostProcBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);

		GetCamera()->Setup({ 0, 50, -150 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 });

		m_Gui.Init(this);
	}

	void FFTOceanDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera(), 50.0f);

		struct PassData
		{
			XMFLOAT4X4 World;
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CameraPos;
			UINT Padding0;
			XMFLOAT3 MainLightPos;
			UINT Padding1;
			XMFLOAT4 MainLightColor;
		} passData;

		XMStoreFloat4x4(&passData.World, XMMatrixIdentity());
		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passData.CameraPos = GetCamera()->GetPosition();
		FillMainLightPosFromSunAngles(passData.MainLightPos);
		passData.MainLightColor = m_Atmosphere->GetSunColor();

		m_PassBuffer->LoadData(GetMainContext(), passData);
	}

	void FFTOceanDemo::OnRender(const Timer& timer)
	{
		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		ID3D12DescriptorHeap* heaps[]{ GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(heaps), heaps);

		m_FFTOcean->Update(timer.GetTotalTime());

		const float clear[4] = { 0, 0, 0, 1 };

		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_AccumulationBuffer->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &m_AccumulationBuffer->GetRTVView()->GetCpuHandle(), true, &GetSwapChain()->DepthStencilView());

		m_DrawPSO.BeginPSOAndCommitResources(GetMainContext(), m_DrawBinder.get());
		
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_MeshGenerator->GetVertexBufferView());
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_MeshGenerator->GetIndexBufferView());

		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_MeshGenerator->GetIndexCount(), 1, 0, 0, 0);

		XMFLOAT3 sunDir{};
		FillMainLightPosFromSunAngles(sunDir);

		m_Atmosphere->Render(GetCamera(), sunDir);

		GetMainContext()->GetCommandCtx()->TransitionResource(m_AccumulationBuffer.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		m_PassBuffer->LoadData(GetMainContext(), m_AccumulationBuffer->GetSRVView()->GetGpuHeapIndex());

		m_PostProcPSO.BeginPSOAndCommitResources(GetMainContext(), m_PostProcBinder.get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		GetMainContext()->GetCommandCtx()->TransitionResource(m_AccumulationBuffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

		m_Gui.RenderImGUI();
	}

	void FFTOceanDemo::OnResize()
	{
		//
		// Create Accumulation Buffer
		//
		{
			uint32 width = GetSwapChain()->GetWidth();
			uint32 height = GetSwapChain()->GetHeight();

			D3D12_RESOURCE_DESC resourceDesc;
			ZeroMemory(&resourceDesc, sizeof(resourceDesc));
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Alignment = 0;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.MipLevels = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.Width = (UINT)width;
			resourceDesc.Height = (UINT)height;
			resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE clearVal;
			clearVal.Color[0] = 0;
			clearVal.Color[1] = 0;
			clearVal.Color[2] = 0;
			clearVal.Color[3] = 1;
			clearVal.Format = resourceDesc.Format;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = resourceDesc.Format;

			m_AccumulationBuffer = std::make_shared<TextureD3D12>(GetDevice(), resourceDesc, &clearVal, QueueId::Direct);
			m_AccumulationBuffer->CreateSRV(&srvDesc, false);
			m_AccumulationBuffer->CreateRTV(nullptr);
			m_AccumulationBuffer->SetName(L"AccumulationBuffer");

			GetMainContext()->GetCommandCtx()->TransitionResource(m_AccumulationBuffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
	}
}