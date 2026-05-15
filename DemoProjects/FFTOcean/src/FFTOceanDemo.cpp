#include "FFTOceanDemo.h"

#include <DemoHelpers.h>
#include <d3dx12.h>

#include <algorithm>
#include <cmath>

namespace EduEngine

{
	namespace
	{
		constexpr int kFftTextureSizeChoices[] = { 64, 128, 256, 512, 1024, 2048 };
		constexpr int kFftTextureSizeCount = static_cast<int>(sizeof(kFftTextureSizeChoices) / sizeof(kFftTextureSizeChoices[0]));

		int IndexOfTextureSize(uint32 textureSize)
		{
			for (int i = 0; i < kFftTextureSizeCount; ++i)
				if (static_cast<uint32>(kFftTextureSizeChoices[i]) == textureSize)
					return i;
			return 3;
		}
	}

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

		ShaderResourceDesc resDesc[] = 
		{
			{ "cbPass", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto hs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"Hull", L"hs_6_6", nullptr, sDesc);
		auto ds = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"Domain", L"ds_6_6", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

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
		m_DrawPSO.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_DrawPSO.SetDepthStencilState(dss);
		m_DrawPSO.SetInputLayout(inputLayout);
		m_DrawPSO.Build(GetDevice());

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

		GetCamera()->Setup({ 0, 50, -150 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 });
	}

	void FFTOceanDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera(), 50.0f);

		struct PassData
		{
			XMFLOAT4X4 World;
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CameraPos;
			UINT Padding;
			XMFLOAT3 MainLightPos;
		} passData;

		XMStoreFloat4x4(&passData.World, XMMatrixIdentity());
		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passData.CameraPos = GetCamera()->GetPosition();
		FillMainLightPosFromSunAngles(passData.MainLightPos);

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
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		m_DrawPSO.BeginPSOAndCommitResources(GetMainContext(), m_DrawBinder.get());
		
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_MeshGenerator->GetVertexBufferView());
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_MeshGenerator->GetIndexBufferView());

		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_MeshGenerator->GetIndexCount(), 1, 0, 0, 0);

		RenderGui();
	}

	void FFTOceanDemo::RenderGui()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(560, 680), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(420, 360), ImVec2(FLT_MAX, FLT_MAX));

		ImGui::Begin("Settings", nullptr);

		if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_FittingPolicyScroll))
		{
			if (ImGui::BeginTabItem("FFT & cascades"))
			{
				ImGui::BeginChild("##_fft_scroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

				ImGui::SeparatorText("FFT grid & cascades");
				{
					bool initialChanged = false;

					int texIdx = IndexOfTextureSize(m_OceanInitialSettings.TextureSize);
					const char* fftSizeLabels[] = { "64", "128", "256", "512", "1024", "2048" };
					if (ImGui::Combo("FFT texture size (px)", &texIdx, fftSizeLabels, kFftTextureSizeCount))
					{
						texIdx = std::clamp(texIdx, 0, kFftTextureSizeCount - 1);
						uint32 newSize = static_cast<uint32>(kFftTextureSizeChoices[texIdx]);
						if (newSize != m_OceanInitialSettings.TextureSize)
						{
							m_OceanInitialSettings.TextureSize = newSize;
							initialChanged = true;
						}
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Power-of-two simulation resolution; recreates ocean resources.");

					int cascadeCount = static_cast<int>(m_OceanInitialSettings.CascadesCount);
					if (ImGui::DragInt("Cascade count", &cascadeCount, 1, 1, static_cast<int>(FFTOcean::MaxCascades)))
					{
						cascadeCount = std::clamp(cascadeCount, 1, static_cast<int>(FFTOcean::MaxCascades));
						if (static_cast<uint32>(cascadeCount) != m_OceanInitialSettings.CascadesCount)
						{
							m_OceanInitialSettings.CascadesCount = static_cast<uint32>(cascadeCount);
							initialChanged = true;
						}
					}

					for (uint32 ci = 0; ci < m_OceanInitialSettings.CascadesCount; ++ci)
					{
						ImGui::PushID(static_cast<int>(ci));
						WaterCascade& c = m_OceanInitialSettings.Cascades[ci];
						if (ImGui::TreeNodeEx((void*)(uintptr_t)ci, ImGuiTreeNodeFlags_DefaultOpen, "Cascade %u", static_cast<unsigned>(ci)))
						{
							initialChanged |= ImGui::DragFloat("Wavelength (m)", &c.Wavelength, 1.0f, 1.0f, 1.0e6f);
							initialChanged |= ImGui::DragFloat("Cutoff high", &c.CutoffHigh, std::max(c.CutoffHigh * 0.02f, 1.0f), 1.0f, 1.0e15f, "%.3e");
							initialChanged |= ImGui::DragFloat("Cutoff low", &c.CutoffLow, std::max(c.CutoffLow * 0.05f, 1e-12f), 1e-15f, 1.0e6f, "%.3e");
							initialChanged |= ImGui::DragFloat("Swell", &c.Swell, 0.01f, 0.0f, 1.0f);
							initialChanged |= ImGui::DragFloat("Fade", &c.Fade, 0.01f, 0.0f, 1.0f);
							ImGui::TreePop();
						}
						ImGui::PopID();
					}

					if (initialChanged)
						RecreateFFTOceanPreservingSettings();
				}

				ImGui::PopStyleVar();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Ocean spectrum"))
			{
				ImGui::BeginChild("##_ocean_scroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

				ImGui::SeparatorText("Ocean (FFT spectrum)");
				{
					FFTOcean::Settings ocean = m_FFTOcean->GetSettings();
					bool oceanChanged = false;

					oceanChanged |= ImGui::DragFloat("Wind speed (m/s)", &ocean.WindSpeed, 0.5f, 0.1f, 120.0f);
					oceanChanged |= ImGui::DragFloat("Wind direction X", &ocean.WindDirectionX, 0.01f, 0.0f, 0.0f);
					oceanChanged |= ImGui::DragFloat("Wind direction Y", &ocean.WindDirectionY, 0.01f, 0.0f, 0.0f);
					oceanChanged |= ImGui::DragFloat("Gravity (m/s^2)", &ocean.Gravity, 0.01f, 0.1f, 25.0f);
					oceanChanged |= ImGui::DragFloat("Fetch (m)", &ocean.Fetch, 100.0f, 1000.0f, 2.0e6f);
					oceanChanged |= ImGui::DragFloat("Depth (m)", &ocean.Depth, 10.0f, 1.0f, 20000.0f);

					if (oceanChanged)
						m_FFTOcean->UpdateSettings(ocean);
				}

				ImGui::PopStyleVar();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Tessellation"))
			{
				ImGui::BeginChild("##_tess_scroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

				ImGui::SeparatorText("Tessellation & culling");
				{
					bool constantsChanged = false;

					constantsChanged |= ImGui::DragInt("Max LOD level", (int*)&m_ConstantsData.MaxLODLevel, 1, 0, 16);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Which displacement cascade band is used vs distance.");

					constantsChanged |= ImGui::DragInt("Max tessellation factor", (int*)&m_ConstantsData.TesselationLevel, 1, 1, 100);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Target hull edge factor when the patch is at the reference distance.");

					constantsChanged |= ImGui::DragFloat("Full tessellation distance", &m_ConstantsData.MaxTesselationDistance, 1.0f, 1.0f, 10000.0f);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Camera distance at which edges reach max tessellation.");

					constantsChanged |= ImGui::DragFloat("Tessellation falloff", &m_ConstantsData.TesselationDecayFactor, 0.05f, 0.1f, 20.0f);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Higher keeps dense tessellation closer to the camera.");

					constantsChanged |= ImGui::DragFloat("Frustum / patch margin", &m_ConstantsData.CullingTollerance, 0.1f, 0.0f, 50.0f);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Extra world units around patch bounds before hull culling.");

					if (constantsChanged)
						m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
				}

				ImGui::PopStyleVar();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Shading & BRDF"))
			{
				ImGui::BeginChild("##_shade_scroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

				ImGui::SeparatorText("Sun direction");
				ImGui::SliderFloat("Azimuth (deg)", &m_SunAzimuthDegrees, 0.f, 360.f, "%.1f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Horizontal angle: 0 deg = +X, 90 deg = +Z, 180 deg = -X (Y is up).");

				ImGui::SliderFloat("Elevation (deg)", &m_SunElevationDegrees, -20.f, 88.f, "%.1f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Angle above horizon: 0 at horizon, 90 at zenith. Slightly negative = sun below horizon.");

				{
					XMFLOAT3 dir{};
					FillMainLightPosFromSunAngles(dir);
					const float invLen = 1.f / sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
					dir.x *= invLen;
					dir.y *= invLen;
					dir.z *= invLen;
					ImGui::TextDisabled("Light direction (unit): %.3f, %.3f, %.3f", dir.x, dir.y, dir.z);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Water shading & BRDF");
				{
					bool shadingChanged = false;

					shadingChanged |= ImGui::ColorEdit3("Main light color", &m_ConstantsData.MainLightColor.x);
					shadingChanged |= ImGui::DragFloat("Environment reflection", &m_ConstantsData.EnvironmentReflectionStrength, 0.01f, 0.0f, 4.0f);

					shadingChanged |= ImGui::ColorEdit3("Subsurface scatter color", &m_ConstantsData.SubsurfaceScatteringColor.x);
					shadingChanged |= ImGui::DragFloat("Subsurface scatter intensity", &m_ConstantsData.SubsurfaceScatteringIntensity, 0.001f, 0.0f, 1.0f);

					shadingChanged |= ImGui::ColorEdit3("Deep water tint", &m_ConstantsData.DeepWaterColor.x);
					shadingChanged |= ImGui::DragFloat("Water fog density", &m_ConstantsData.WaterFogDensity, 0.01f, 0.0f, 2.0f);
					shadingChanged |= ImGui::DragFloat("Refraction strength", &m_ConstantsData.RefractionStrength, 0.01f, 0.0f, 1.0f);

					shadingChanged |= ImGui::DragFloat("Roughness", &m_ConstantsData.Roughness, 0.005f, 0.02f, 1.0f);
					shadingChanged |= ImGui::DragFloat("Anisotropy EX", &m_ConstantsData.AnisoEX, 0.01f, 0.01f, 2.0f);
					shadingChanged |= ImGui::DragFloat("Anisotropy EY", &m_ConstantsData.AnisoEY, 0.01f, 0.01f, 2.0f);

					shadingChanged |= ImGui::DragFloat("Foam blend", &m_ConstantsData.FoamBlending, 0.01f, 0.0f, 1.0f);
					shadingChanged |= ImGui::DragFloat("Foam threshold", &m_ConstantsData.FoamThreshold, 0.01f, 0.0f, 2.0f);
					shadingChanged |= ImGui::ColorEdit3("Foam color", &m_ConstantsData.FoamColor.x);

					shadingChanged |= ImGui::ColorEdit3("Shadows tint", &m_ConstantsData.ShadowsColor.x);
					shadingChanged |= ImGui::DragFloat("Shadows intensity", &m_ConstantsData.ShadowsIntensity, 0.01f, 0.0f, 1.0f);
					shadingChanged |= ImGui::DragFloat("Sun reflection (specular)", &m_ConstantsData.SunReflectionStrength, 0.01f, 0.0f, 4.0f);

					if (shadingChanged)
						m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
				}

				ImGui::PopStyleVar();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}
}