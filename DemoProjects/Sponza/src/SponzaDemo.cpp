#include "SponzaDemo.h"

#include <DemoHelpers.h>
#include <DirectXPackedVector.h>

using namespace DirectX::PackedVector;

namespace EduEngine
{
	void SponzaDemo::OnStartUp()
	{
		GetCamera()->Setup(
			{ 0, 25, -100 },
			{ 0, 0, 1 },
			{ 1, 0, 0 },
			{ 1, 1, 0 }
		);
		
		MeshLoadDesc meshDesc = {};
		meshDesc.Flags = MESH_LOAD_FLAG_LOAD_TEXTURES;
		meshDesc.TextureBasePath = "assets\\Textures\\";
		meshDesc.TextureExt = ".dds";
		meshDesc.TextureLoadDesc.OnCPU = false;

		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), "assets\\Models\\scene.gltf");
		m_Mesh->Load(meshDesc);

		m_PbrPrepass = std::make_unique<PBRPrepass>(GetDevice(), GetMainContext(), false);
		m_PbrPrepass->GenerateTextures("assets\\Textures\\HDR\\kloofendal_48d_partly_cloudy_puresky_4k.hdr", GetDevice(), GetMainContext());

		m_GBuffer = std::make_unique<GBuffer>(SponzaGBufferId::NumBuffers, SPONZA_G_BUFFERS, 1, ACCUM_BUFFER_FORMAT);
		m_Ssao = std::make_unique<SSAO>(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);

		m_CSMRendering = std::make_unique<CSMRendering>(GetDevice(), GetMainContext());

		m_DebugRenderer = std::make_unique<DebugRendererSystem>(GetDevice());

		DeferredPBRLightPass::MaterialData material = {};
		material.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		m_LightPass = std::make_unique<DeferredPBRLightPass>(GetDevice(), GetMainContext(), ACCUM_BUFFER_FORMAT);
		m_LightPass->SetMaterial(GetMainContext(), material);

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		m_PostProcPso.Name = "Sponza_PostProc";
		m_PostProcPso.DependentParams = { RenderFeatureID::DebugView };
		m_PostProcPso.BuildPsoFunc = [this]() { return BuildPostProcPso(); };
		m_PostProcPso.OnPsoUpdated = [this]()
			{
				m_PostProcBinder = m_PostProcPso.Pso->CreateShaderBinder();
				if (m_GBuffer->GetAccumBufferShared(0))
					m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_GBuffer->GetAccumBufferShared(0));
			};
		m_PostProcPso.Initialize();

		BuildDrawPso();

		m_DrawBinder = m_DrawPso.CreateShaderBinder();
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPerObject", m_ObjBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);

		OnResize();
	}
	
	void SponzaDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera(), 25.0f);

		struct PassData
		{
			XMFLOAT4X4 ViewProj;
		};

		PassData passData;
		
		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		m_PassBuffer->LoadData(GetMainContext(), passData);
		
		m_Ssao->Update(GetCamera(), GetMainContext());
		m_CSMRendering->Update(GetMainContext(), GetCamera(), &m_LightData);
		m_LightPass->Update(GetMainContext(), GetCamera(), &m_LightData, 1, m_CSMRendering.get());
	}

	void SponzaDemo::OnRender(const Timer& timer)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		CSMRendering::RenderObject shadowObjects[]
		{
			{ m_Mesh.get(), XMMatrixScaling(5, 5, 5) },
		};

		m_CSMRendering->Render(GetMainContext(), shadowObjects, 1);

		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		for (uint32 i = 0; i < SponzaGBufferId::NumBuffers; i++)
			GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(i), D3D12_RESOURCE_STATE_RENDER_TARGET);

		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		for (uint32 i = 0; i < SponzaGBufferId::NumBuffers; i++)
			GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_GBuffer->GetGBufferRTVView(i), clear, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]
		{
			m_GBuffer->GetGBufferRTVView(0),
			m_GBuffer->GetGBufferRTVView(1),
			m_GBuffer->GetGBufferRTVView(2),
		};

		GetMainContext()->GetCommandCtx()->SetRenderTargets(SponzaGBufferId::NumBuffers, rtvs, false, &GetSwapChain()->DepthStencilView());

		auto GetTexIdx = [&](Texture* texture) -> UINT
			{
				if (texture)
					return texture->GetD3D12Texture()->GetSRVView()->GetGpuHeapIndex();

				return -1;
			};

		m_DrawPso.CommitPso(GetMainContext());
		for (uint32 i = 0; i < m_Mesh->GetMeshCount(); i++)
		{
			struct ObjData
			{
				XMFLOAT4X4 World;
				UINT AlbedoTexIdx;
				UINT NormalMapIdx;
				UINT MetallicRoughnessIdx;
				UINT AOIdx;
			} objData;

			XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMMatrixScaling(5, 5, 5)));
			objData.AlbedoTexIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_BASE_COLOR));
			objData.NormalMapIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_NORMAL_MAP));
			objData.MetallicRoughnessIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_METALLIC_ROUGHNESS));
			objData.AOIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_AMBIENT_OCCLUSION));

			m_ObjBuffer->LoadData(GetMainContext(), objData);

			m_DrawPso.CommitBinder(GetMainContext(), m_DrawBinder.get());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_Mesh->GetIndexBuffer(i)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_Mesh->GetVertexBuffer(i)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexCount(i), 1, 0, 0, 0);
		}
		
		for (uint32 i = 0; i < SponzaGBufferId::NumBuffers; i++)
			GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(i), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		m_Ssao->Render(GetMainContext());

		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		m_LightPass->Render(GetMainContext(), m_GBuffer->GetAccumBuffer(0));

		//
		// Post process pass
		//
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		m_PostProcPso.Pso->CommitAll(GetMainContext(), m_PostProcBinder.get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		m_PbrPrepass->RenderSky(GetDevice(), GetMainContext(), GetCamera());

		m_DebugRenderer->DrawSphere(10, { 255, 0, 255 }, XMMatrixTranslation(m_LightData.Position.x, m_LightData.Position.y, m_LightData.Position.z), 16 );
		m_DebugRenderer->Render(GetMainContext(), GetCamera()->GetViewProjMatrix(), GetCamera()->GetPosition());

		//
		// GUI
		//

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Editor", nullptr);

		if (ImGui::CollapsingHeader("Shadow Settings"))
		{
			CSMRendering::Settings csmSettings = m_CSMRendering->GetSettings();

			if (ImGui::DragFloat("Shadow Distance", &csmSettings.ShadowDistance, 1.0f, 0.1f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
			{
				m_CSMRendering->UpdateSettings(GetMainContext(), csmSettings);
			}

			if (ImGui::SliderInt("Cascades count", (int*)&csmSettings.CascadesCount, 1, CSMRendering::MAX_CASCADES))
			{
				m_CSMRendering->UpdateSettings(GetMainContext(), csmSettings);
				OnResize(); // TODO: it only needs to update DeferredPBRLightPass::BuffersIndexesData
			}

			for (uint32 i = 0; i < csmSettings.CascadesCount; i++)
			{
				float min = i == 0 ? 0.01f : csmSettings.CSMSplits[i - 1];
				float max = i == csmSettings.CascadesCount - 1 ? 1.0f : csmSettings.CSMSplits[i + 1];

				char splitLabel[16] = {};
				sprintf(splitLabel, "Cascade%d", i);

				if (ImGui::SliderFloat(splitLabel, &csmSettings.CSMSplits[i], min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp))
					m_CSMRendering->UpdateSettings(GetMainContext(), csmSettings);
			}

			if (ImGui::SliderFloat("Depth Bias", &csmSettings.ShadowBias.x, 0.0f, 1.0f))
				m_CSMRendering->UpdateSettings(GetMainContext(), csmSettings);

			if (ImGui::SliderFloat("Normal Bias", &csmSettings.ShadowBias.y, 0.0f, 1.0f))
				m_CSMRendering->UpdateSettings(GetMainContext(), csmSettings);
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
				
				SPONZA_G_BUFFERS[SponzaGBufferId::Normal] = currentPackMethod == 0 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R16G16_FLOAT;
				m_GBuffer = std::make_unique<GBuffer>(SponzaGBufferId::NumBuffers, SPONZA_G_BUFFERS, 1, ACCUM_BUFFER_FORMAT);

				BuildDrawPso();
				g_PsoCache.OnRenderFeaturesChanged(g_RenderFeatures, RenderFeatureID::PackNormalsMethod);

				OnResize();
			}
		}

		ImGui::End();

		RenderEngine::PopulateDebugImguiCommand();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void SponzaDemo::OnResize()
	{
		if (m_GBuffer)
		{
			m_GBuffer->Resize(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);
		}

		if (m_Ssao)
		{
			m_Ssao->Resize(GetViewport().Width, GetViewport().Height);
			m_Ssao->BindResources(m_GBuffer->GetGBufferShared(SponzaGBufferId::Normal), GetSwapChain()->GetDepthStencilTextureShared());

			m_PostProcBinder->DryMutableResources();
			m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_GBuffer->GetAccumBufferShared(0));
		}

		if (m_LightPass)
		{
			m_GpuCopyDescriptors = std::move(GetDevice()->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5));

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptors.GetCpuHandle(0),
				m_GBuffer->GetGBuffer(SponzaGBufferId::Albedo)->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptors.GetCpuHandle(1),
				m_GBuffer->GetGBuffer(SponzaGBufferId::Normal)->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptors.GetCpuHandle(2),
				m_GBuffer->GetGBuffer(SponzaGBufferId::MetalRoughAo)->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptors.GetCpuHandle(3),
				GetSwapChain()->GetDepthStencilTextureShared()->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptors.GetCpuHandle(4),
				m_Ssao->GetSSAOMap()->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			DeferredPBRLightPass::BuffersIndexesData deferredLightBuffers = {};
			deferredLightBuffers.AlbedoIdx = m_GpuCopyDescriptors.GetGpuHeapIndex(0);
			deferredLightBuffers.NormalIdx = m_GpuCopyDescriptors.GetGpuHeapIndex(1);
			deferredLightBuffers.MetallicRoughAoIdx = m_GpuCopyDescriptors.GetGpuHeapIndex(2);
			deferredLightBuffers.DepthIdx = m_GpuCopyDescriptors.GetGpuHeapIndex(3);
			deferredLightBuffers.SsaoMapIdx = m_GpuCopyDescriptors.GetGpuHeapIndex(4);
			deferredLightBuffers.IrradianceMapIdx = m_PbrPrepass->GetIrradianceMap()->GetSRVView()->GetGpuHeapIndex();
			deferredLightBuffers.PrefilteredMapIdx = m_PbrPrepass->GetPrefilteredMap()->GetSRVView()->GetGpuHeapIndex();
			deferredLightBuffers.BRDFLutIdx = m_PbrPrepass->GetBrdfLut()->GetSRVView()->GetGpuHeapIndex();

			for (uint32 i = 0; i < m_CSMRendering->GetCascadeCount(); i++)
				deferredLightBuffers.ShadowMapIdx[i] = m_CSMRendering->GetSrv(i)->GetGpuHeapIndex();

			m_LightPass->SetBufferIndexes(GetMainContext(), deferredLightBuffers);
		}
	}

	void SponzaDemo::BuildDrawPso()
	{
		ShaderResourceDesc sRes[]
		{
			ShaderResourceDesc("cbPerObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(sRes);
		sDesc.ResourceDesc = sRes;

		auto packNormal = std::to_wstring((int)g_RenderFeatures.PackNormalsMethod);

		LPCWSTR macrosBuff[]
		{
			L"PACK_NORMALS", packNormal.c_str(),
			NULL, NULL,
		};

		auto drawVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\GeometryPass.hlsl", L"VS", L"vs_6_6", macrosBuff, sDesc);
		auto drawPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\GeometryPass.hlsl", L"PS", L"ps_6_6", macrosBuff, sDesc);

		D3D12_INPUT_ELEMENT_DESC inputLayout[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

		m_DrawPso.SetDepthStencilState(dss);
		m_DrawPso.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_DrawPso.SetShader(drawVS);
		m_DrawPso.SetShader(drawPS);
		m_DrawPso.SetRTVFormats(SponzaGBufferId::NumBuffers, SPONZA_G_BUFFERS);
		m_DrawPso.Build(GetDevice());
	}

	std::shared_ptr<PipelineStateBase> SponzaDemo::BuildPostProcPso()
	{
		ShaderResourceDesc sRes[]
		{
			ShaderResourceDesc("cbPerObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(sRes);
		sDesc.ResourceDesc = sRes;

		auto debugViewStr = std::to_wstring((int)g_RenderFeatures.DebugView);

		LPCWSTR macrosBuff[]
		{
			L"DEBUG_VIEW", debugViewStr.c_str(),
			NULL, NULL,
		};

		auto fsQuadVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", macrosBuff, sDesc);
		auto postProcPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PostProc.hlsl", L"PS", L"ps_6_0", macrosBuff, sDesc);

		D3D12_DEPTH_STENCIL_DESC dssOff = {};
		dssOff.DepthEnable = false;

		auto pso = std::make_shared<PipelineState>();
		pso->SetDepthStencilState(dssOff);
		pso->SetShader(fsQuadVS);
		pso->SetShader(postProcPS);
		pso->SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		pso->Build(GetDevice());

		return pso;
	}
}