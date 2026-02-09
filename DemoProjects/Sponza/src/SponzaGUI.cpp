#include "SponzaGUI.h"
#include "SponzaDemo.h"

namespace EduEngine
{
	SponzaGUI::SponzaGUI() :
		m_Sponza(nullptr)
	{
		memset(m_ActiveReflections, 0, sizeof(bool) * ReflectionProbesManager::MAX_REFLECTION_PROBES);
	}

	void SponzaGUI::Init(SponzaDemo* parent)
	{
		m_Sponza = parent;
	}

	void SponzaGUI::RenderImGUI()
	{
		ReflectionProbe* probeToBake = nullptr;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Editor", nullptr);

		if (ImGui::CollapsingHeader("Shadow Settings"))
		{
			CSMRendering::Settings csmSettings = m_Sponza->m_CSMRendering->GetSettings();

			if (ImGui::DragFloat("Shadow Distance", &csmSettings.ShadowDistance, 1.0f, 0.1f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
			{
				m_Sponza->m_CSMRendering->UpdateSettings(m_Sponza->GetMainContext(), csmSettings);
			}

			if (ImGui::SliderInt("Cascades count", (int*)&csmSettings.CascadesCount, 1, CSMRendering::MAX_CASCADES))
			{
				m_Sponza->m_CSMRendering->UpdateSettings(m_Sponza->GetMainContext(), csmSettings);
				m_Sponza->OnResize(); // TODO: it only needs to update DeferredPBRLightPass::BuffersIndexesData
			}

			for (uint32 i = 0; i < csmSettings.CascadesCount; i++)
			{
				float min = i == 0 ? 0.01f : csmSettings.CSMSplits[i - 1];
				float max = i == csmSettings.CascadesCount - 1 ? 1.0f : csmSettings.CSMSplits[i + 1];

				char splitLabel[16] = {};
				sprintf(splitLabel, "Cascade%d", i);

				if (ImGui::SliderFloat(splitLabel, &csmSettings.CSMSplits[i], min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp))
					m_Sponza->m_CSMRendering->UpdateSettings(m_Sponza->GetMainContext(), csmSettings);
			}

			if (ImGui::SliderFloat("Depth Bias", &csmSettings.ShadowBias.x, 0.0f, 1.0f))
				m_Sponza->m_CSMRendering->UpdateSettings(m_Sponza->GetMainContext(), csmSettings);

			if (ImGui::SliderFloat("Normal Bias", &csmSettings.ShadowBias.y, 0.0f, 1.0f))
				m_Sponza->m_CSMRendering->UpdateSettings(m_Sponza->GetMainContext(), csmSettings);
		}

		if (ImGui::CollapsingHeader("SSAO"))
		{
			SSAO::Settings ssaoSettings = m_Sponza->m_Ssao->GetSettings();

			if (ImGui::SliderFloat("Occlusion Radius", &ssaoSettings.OcclusionRadius, 0.0f, 1.0f))
				m_Sponza->m_Ssao->UpdateSettings(ssaoSettings);

			if (ImGui::SliderFloat("Occlusion Fade Start", &ssaoSettings.OcclusionFadeStart, 0.0f, 1.0f))
				m_Sponza->m_Ssao->UpdateSettings(ssaoSettings);

			if (ImGui::SliderFloat("Occlusion Fade End", &ssaoSettings.OcclusionFadeEnd, 0.0f, 1.0f))
				m_Sponza->m_Ssao->UpdateSettings(ssaoSettings);

			if (ImGui::SliderFloat("Surface Epsilon", &ssaoSettings.SurfaceEpsilon, 0.0f, 0.1f))
				m_Sponza->m_Ssao->UpdateSettings(ssaoSettings);
		}

		if (ImGui::CollapsingHeader("Reflection Probes"))
		{
			if (ImGui::Button("Add New"))
			{
				ReflectionProbe::Settings probeSettings = {};
				probeSettings.Flags = ReflectionProbe::Flags::CREATE_IRRADIANCE_MAP | ReflectionProbe::Flags::CREATE_PREFILTERED_MAP;
				m_Sponza->m_ReflectionProbeMgr->Add(m_Sponza->GetMainContext(), probeSettings);
				m_Sponza->m_ReflectionProbeMgr->RebuildBuffer(m_Sponza->GetMainContext());
			}

			for (uint32 i = 0; i < m_Sponza->m_ReflectionProbeMgr->Count(); i++)
			{
				ImGui::PushID(i);

				if (m_ActiveReflections[i] = ImGui::CollapsingHeader("Probe"))
				{
					auto rp = m_Sponza->m_ReflectionProbeMgr->GetReflectionProbe(i);
					XMFLOAT3 center = rp->GetCenter();
					XMFLOAT3 boxSize = rp->GetExtents();

					if (ImGui::DragFloat3("Position", (float*)&center))
						rp->SetCenter(center);

					if (ImGui::DragFloat3("Box Size", (float*)&boxSize))
						rp->SetExtents(boxSize);

					if (ImGui::Button("Bake"))
					{
						probeToBake = rp;
					}

					if (ImGui::Button("Remove"))
					{
						m_Sponza->m_ReflectionProbeMgr->RemoveAt(i);
						m_Sponza->m_ReflectionProbeMgr->RebuildBuffer(m_Sponza->GetMainContext());
					}
				}

				ImGui::PopID();
			}
		}

		if (ImGui::CollapsingHeader("Debug View"))
		{
			static int currentView = 0;

			if (ImGui::Combo("Type##DebugView", &currentView, DebugViewsStr, IM_ARRAYSIZE(DebugViewsStr)))
			{
				g_RenderFeatures.DebugView = (DebugView)currentView;
				g_PsoCache.OnRenderFeaturesChanged(g_RenderFeatures, RenderFeatureID::DebugView);
			}
		}

		if (ImGui::CollapsingHeader("Pack Normals"))
		{
			static int currentPackMethod = 0;

			if (ImGui::Combo("Type##PackNormals", &currentPackMethod, PackNormalsMethodStr, IM_ARRAYSIZE(PackNormalsMethodStr)))
			{
				g_RenderFeatures.PackNormalsMethod = (PackNormalsMethod)currentPackMethod;

				m_Sponza->SPONZA_G_BUFFERS[SponzaGBufferId::Normal] = currentPackMethod == 0 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R16G16_FLOAT;
				m_Sponza->m_GBuffer = std::make_unique<GBuffer>(SponzaGBufferId::NumBuffers, m_Sponza->SPONZA_G_BUFFERS, 1, ACCUM_BUFFER_FORMAT);

				m_Sponza->BuildDrawPso();
				g_PsoCache.OnRenderFeaturesChanged(g_RenderFeatures, RenderFeatureID::PackNormalsMethod);

				m_Sponza->OnResize();
			}
		}

		ImGui::End();

		m_Sponza->RenderEngine::PopulateDebugImguiCommand();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_Sponza->GetMainContext()->GetCommandCtx()->GetCmdList());

		if (probeToBake)
		{
			probeToBake->Bake(m_Sponza->GetMainContext(), m_Sponza->m_PbrPrepass.get(), m_Sponza->m_Skybox.get(), &m_Sponza->m_LightData, 1, m_Sponza->m_RenderObjects.data(), m_Sponza->m_RenderObjects.size());
			m_Sponza->m_ReflectionProbeMgr->RebuildBuffer(m_Sponza->GetMainContext());
			m_Sponza->OnResize();
		}
	}

	void SponzaGUI::DebugDrawReflectionProbes()
	{
		for (uint32 i = 0; i < m_Sponza->m_ReflectionProbeMgr->Count(); i++)
		{
			if (!m_ActiveReflections[i])
				continue;

			const ReflectionProbe* rp = m_Sponza->m_ReflectionProbeMgr->GetReflectionProbe(i);

			BoundingBox probeBox = {};
			probeBox.Center = rp->GetCenter();
			probeBox.Extents = rp->GetExtents();

			m_Sponza->m_DebugRenderer->DrawPoint(probeBox.Center, 5, { 0, 1, 0 });
			m_Sponza->m_DebugRenderer->DrawBoundingBox(probeBox, XMMatrixIdentity(), { 0, 1, 0 });
		}
	}
}