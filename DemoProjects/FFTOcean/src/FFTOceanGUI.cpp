#include "FFTOceanGUI.h"
#include "FFTOceanDemo.h"

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

	bool DragFloat3Coeff(const char* label, XMFLOAT3& value)
	{
		const float maxAbs = std::max({ std::fabs(value.x), std::fabs(value.y), std::fabs(value.z) });
		float speed = maxAbs * 0.05f;
		if (speed < 1e-9f)
			speed = 1e-9f;
		return ImGui::DragFloat3(label, &value.x, speed, 0.0f, 0.0f, "%.3e");
	}
}

namespace EduEngine
{
	void FFTOceanGUI::Init(FFTOceanDemo* parent)
	{
		m_FFTOceanDemo = parent;
	}

	void FFTOceanGUI::RenderImGUI()
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

					int texIdx = IndexOfTextureSize(m_FFTOceanDemo->m_OceanInitialSettings.TextureSize);
					const char* fftSizeLabels[] = { "64", "128", "256", "512", "1024", "2048" };
					if (ImGui::Combo("FFT texture size (px)", &texIdx, fftSizeLabels, kFftTextureSizeCount))
					{
						texIdx = std::clamp(texIdx, 0, kFftTextureSizeCount - 1);
						uint32 newSize = static_cast<uint32>(kFftTextureSizeChoices[texIdx]);
						if (newSize != m_FFTOceanDemo->m_OceanInitialSettings.TextureSize)
						{
							m_FFTOceanDemo->m_OceanInitialSettings.TextureSize = newSize;
							initialChanged = true;
						}
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Power-of-two simulation resolution; recreates ocean resources.");

					int cascadeCount = static_cast<int>(m_FFTOceanDemo->m_OceanInitialSettings.CascadesCount);
					if (ImGui::DragInt("Cascade count", &cascadeCount, 1, 1, static_cast<int>(FFTOcean::MaxCascades)))
					{
						cascadeCount = std::clamp(cascadeCount, 1, static_cast<int>(FFTOcean::MaxCascades));
						if (static_cast<uint32>(cascadeCount) != m_FFTOceanDemo->m_OceanInitialSettings.CascadesCount)
						{
							m_FFTOceanDemo->m_OceanInitialSettings.CascadesCount = static_cast<uint32>(cascadeCount);
							initialChanged = true;
						}
					}

					for (uint32 ci = 0; ci < m_FFTOceanDemo->m_OceanInitialSettings.CascadesCount; ++ci)
					{
						ImGui::PushID(static_cast<int>(ci));
						WaterCascade& c = m_FFTOceanDemo->m_OceanInitialSettings.Cascades[ci];
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
						m_FFTOceanDemo->RecreateFFTOceanPreservingSettings();
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
					FFTOcean::Settings ocean = m_FFTOceanDemo->m_FFTOcean->GetSettings();
					bool oceanChanged = false;

					oceanChanged |= ImGui::DragFloat("Wind speed (m/s)", &ocean.WindSpeed, 0.5f, 0.1f, 120.0f);
					oceanChanged |= ImGui::DragFloat("Wind direction X", &ocean.WindDirectionX, 0.01f, 0.0f, 0.0f);
					oceanChanged |= ImGui::DragFloat("Wind direction Y", &ocean.WindDirectionY, 0.01f, 0.0f, 0.0f);
					oceanChanged |= ImGui::DragFloat("Gravity (m/s^2)", &ocean.Gravity, 0.01f, 0.1f, 25.0f);
					oceanChanged |= ImGui::DragFloat("Fetch (m)", &ocean.Fetch, 100.0f, 1000.0f, 2.0e6f);
					oceanChanged |= ImGui::DragFloat("Depth (m)", &ocean.Depth, 10.0f, 1.0f, 20000.0f);

					if (oceanChanged)
						m_FFTOceanDemo->m_FFTOcean->UpdateSettings(ocean);
				}

				ImGui::PopStyleVar();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Atmosphere"))
			{
				static Settings m_PendingSettings = {};
				static bool m_SettingsDirty = false;

				Settings m_Settings = m_FFTOceanDemo->m_Atmosphere->GetSettings();

				auto TryApplyPendingSettings = [&]()
					{
						if (!m_SettingsDirty || ImGui::IsAnyItemActive())
							return;

						m_FFTOceanDemo->m_Atmosphere->UpdateSettings(m_PendingSettings);
						m_SettingsDirty = false;
					};

				ImGui::BeginChild("##_atmo_scroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

				Settings settings = m_SettingsDirty ? m_PendingSettings : m_Settings;
				bool changed = false;

				ImGui::TextDisabled("LUTs update when you release a control (avoids GPU sync while dragging).");

				changed |= ImGui::SliderFloat("Sun size (degrees)", &settings.SunSize, 0.01f, 1.0f, "%.2f");

				ImGui::SeparatorText("Planet & atmosphere shell");
				changed |= ImGui::DragFloat("Planet radius (m)", &settings.PlanetRadius, 1000.0f, 1.0e6f, 1.0e8f, "%.0f");
				changed |= ImGui::DragFloat("Atmosphere radius (m)", &settings.AtmosphereRadius, 1000.0f, 1.0e6f, 1.0e8f, "%.0f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Outer boundary of the atmospheric shell (meters from planet center).");

				ImGui::Spacing();
				ImGui::SeparatorText("Mie phase function");
				changed |= ImGui::DragFloat("Mie g (anisotropy)", &settings.MieG, 0.01f, -0.999f, 0.999f, "%.3f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Henyey-Greenstein asymmetry; positive = forward scattering.");

				ImGui::Spacing();
				ImGui::SeparatorText("Rayleigh");
				ImGui::PushID("Rayleigh");
				changed |= DragFloat3Coeff("Scattering coefficient", settings.RayleighScatteringCoefficient);
				changed |= DragFloat3Coeff("Absorption coefficient", settings.RayleighAbsorptionCoefficient);
				changed |= ImGui::DragFloat("Scale height (m)", &settings.RayleighScaleHeight, 50.0f, 100.0f, 50000.0f, "%.0f");
				ImGui::PopID();

				ImGui::Spacing();
				ImGui::SeparatorText("Mie aerosols");
				ImGui::PushID("Mie");
				changed |= DragFloat3Coeff("Scattering coefficient", settings.MieScatteringCoefficient);
				changed |= DragFloat3Coeff("Absorption coefficient", settings.MieAbsorptionCoefficient);
				changed |= ImGui::DragFloat("Scale height (m)", &settings.MieScaleHeight, 10.0f, 10.0f, 10000.0f, "%.0f");
				ImGui::PopID();

				ImGui::Spacing();
				ImGui::SeparatorText("Ozone");
				ImGui::PushID("Ozone");
				changed |= DragFloat3Coeff("Scattering coefficient", settings.OzoneScatteringCoefficient);
				changed |= DragFloat3Coeff("Absorption coefficient", settings.OzoneAbsorptionCoefficient);
				ImGui::PopID();

				ImGui::Spacing();
				ImGui::SeparatorText("Ground");
				ImGui::PushID("Ground");
				changed |= DragFloat3Coeff("Spectrum albedo", settings.GroundSpectrumAlbedo);
				ImGui::PopID();

				if (changed)
				{
					m_PendingSettings = settings;
					m_SettingsDirty = true;
				}

				{
					const XMFLOAT4 sunColor = m_FFTOceanDemo->m_Atmosphere->GetSunColor();
					ImGui::Spacing();
					ImGui::TextDisabled("Sun color (from transmittance LUT): %.3f, %.3f, %.3f", sunColor.x, sunColor.y, sunColor.z);
				}

				if (ImGui::Button("Reset to defaults"))
				{
					m_PendingSettings = Settings{};
					m_SettingsDirty = true;
					changed = true;
				}

				TryApplyPendingSettings();

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

					constantsChanged |= ImGui::DragInt("Max LOD level", (int*)&m_FFTOceanDemo->m_ConstantsData.MaxLODLevel, 1, 0, 16);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Which displacement cascade band is used vs distance.");

					constantsChanged |= ImGui::DragInt("Max tessellation factor", (int*)&m_FFTOceanDemo->m_ConstantsData.TesselationLevel, 1, 1, 100);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Target hull edge factor when the patch is at the reference distance.");

					constantsChanged |= ImGui::DragFloat("Full tessellation distance", &m_FFTOceanDemo->m_ConstantsData.MaxTesselationDistance, 1.0f, 1.0f, 10000.0f);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Camera distance at which edges reach max tessellation.");

					constantsChanged |= ImGui::DragFloat("Tessellation falloff", &m_FFTOceanDemo->m_ConstantsData.TesselationDecayFactor, 0.05f, 0.1f, 20.0f);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Higher keeps dense tessellation closer to the camera.");

					constantsChanged |= ImGui::DragFloat("Frustum / patch margin", &m_FFTOceanDemo->m_ConstantsData.CullingTollerance, 0.1f, 0.0f, 50.0f);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Extra world units around patch bounds before hull culling.");

					if (constantsChanged)
						m_FFTOceanDemo->m_ConstantsBuffer->LoadData(m_FFTOceanDemo->GetMainContext(), &m_FFTOceanDemo->m_ConstantsData);
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
				ImGui::SliderFloat("Azimuth (deg)", &m_FFTOceanDemo->m_SunAzimuthDegrees, 0.f, 360.f, "%.1f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Horizontal angle: 0 deg = +X, 90 deg = +Z, 180 deg = -X (Y is up).");

				ImGui::SliderFloat("Elevation (deg)", &m_FFTOceanDemo->m_SunElevationDegrees, -20.f, 88.f, "%.1f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Angle above horizon: 0 at horizon, 90 at zenith. Slightly negative = sun below horizon.");

				{
					XMFLOAT3 dir{};
					m_FFTOceanDemo->FillMainLightPosFromSunAngles(dir);
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

					shadingChanged |= ImGui::DragFloat("Environment reflection", &m_FFTOceanDemo->m_ConstantsData.EnvironmentReflectionStrength, 0.01f, 0.0f, 4.0f);

					shadingChanged |= ImGui::ColorEdit3("Subsurface scatter color", &m_FFTOceanDemo->m_ConstantsData.SubsurfaceScatteringColor.x);
					shadingChanged |= ImGui::DragFloat("Subsurface scatter intensity", &m_FFTOceanDemo->m_ConstantsData.SubsurfaceScatteringIntensity, 0.001f, 0.0f, 1.0f);

					shadingChanged |= ImGui::ColorEdit3("Deep water tint", &m_FFTOceanDemo->m_ConstantsData.DeepWaterColor.x);
					shadingChanged |= ImGui::DragFloat("Water fog density", &m_FFTOceanDemo->m_ConstantsData.WaterFogDensity, 0.01f, 0.0f, 2.0f);
					shadingChanged |= ImGui::DragFloat("Refraction strength", &m_FFTOceanDemo->m_ConstantsData.RefractionStrength, 0.01f, 0.0f, 1.0f);

					shadingChanged |= ImGui::DragFloat("Roughness", &m_FFTOceanDemo->m_ConstantsData.Roughness, 0.005f, 0.02f, 1.0f);
					shadingChanged |= ImGui::DragFloat("Anisotropy EX", &m_FFTOceanDemo->m_ConstantsData.AnisoEX, 0.01f, 0.01f, 2.0f);
					shadingChanged |= ImGui::DragFloat("Anisotropy EY", &m_FFTOceanDemo->m_ConstantsData.AnisoEY, 0.01f, 0.01f, 2.0f);

					shadingChanged |= ImGui::DragFloat("Foam blend", &m_FFTOceanDemo->m_ConstantsData.FoamBlending, 0.01f, 0.0f, 1.0f);
					shadingChanged |= ImGui::DragFloat("Foam threshold", &m_FFTOceanDemo->m_ConstantsData.FoamThreshold, 0.01f, 0.0f, 2.0f);
					shadingChanged |= ImGui::ColorEdit3("Foam color", &m_FFTOceanDemo->m_ConstantsData.FoamColor.x);

					shadingChanged |= ImGui::ColorEdit3("Shadows tint", &m_FFTOceanDemo->m_ConstantsData.ShadowsColor.x);
					shadingChanged |= ImGui::DragFloat("Shadows intensity", &m_FFTOceanDemo->m_ConstantsData.ShadowsIntensity, 0.01f, 0.0f, 1.0f);
					shadingChanged |= ImGui::DragFloat("Sun reflection (specular)", &m_FFTOceanDemo->m_ConstantsData.SunReflectionStrength, 0.01f, 0.0f, 4.0f);
					
					if (shadingChanged)
						m_FFTOceanDemo->m_ConstantsBuffer->LoadData(m_FFTOceanDemo->GetMainContext(), &m_FFTOceanDemo->m_ConstantsData);
				}

				ImGui::PopStyleVar();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_FFTOceanDemo->GetMainContext()->GetCommandCtx()->GetCmdList());
	}
}