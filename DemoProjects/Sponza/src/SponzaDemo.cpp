#include "SponzaDemo.h"

#include <DemoHelpers.h>
#include <DirectXPackedVector.h>

using namespace DirectX::PackedVector;

namespace EduEngine
{
	void SponzaDemo::OnStartUp()
	{
		m_GUI.Init(this);

		GetCamera()->Setup(
			{ -30, 15, 0 },
			{ 1, 0, 0 },
			{ 0, 0, -1 },
			{ 0, 1, 0 }
		);
		
		MeshLoadDesc meshDesc = {};
		meshDesc.Flags = MESH_LOAD_FLAG_LOAD_TEXTURES;
		meshDesc.TextureBasePath = "assets\\Textures\\";
		meshDesc.TextureExt = ".dds";
		meshDesc.TextureLoadDesc.OnCPU = false;

		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), "assets\\Models\\scene.gltf");
		m_Mesh->Load(meshDesc);

		m_RenderObjects = { RenderObject{ m_Mesh.get(), XMMatrixScaling(5, 5, 5) } };

		m_IBLRendering = std::make_unique<IBLRendering>(GetDevice(), GetMainContext());
		m_Skybox = std::make_unique<Skybox>("assets\\Textures\\HDR\\kloofendal_48d_partly_cloudy_puresky_4k.hdr",
			GetDevice(), GetMainContext(), m_IBLRendering.get());

		m_GBuffer = std::make_unique<GBuffer>(SponzaGBufferId::NumBuffers, SPONZA_G_BUFFERS, 1, ACCUM_BUFFER_FORMAT);
		m_Ssao = std::make_unique<SSAO>(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);

		m_CSMRendering = std::make_unique<CSMRendering>(GetDevice(), GetMainContext());
		m_SSR = std::make_unique<ScreenSpaceReflection>(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);

		struct ProbeInitData
		{
			XMFLOAT3 Position;
			XMFLOAT3 Extents;
		};

		ProbeInitData probes[15]
		{
			{ { -2.0f, 28.0f, 1.5f }, { 42.0f, 11.0f, 11.0f } },
			{ {-49.0f, 26.0f, 1.5f }, {  9.0f, 10.0f, 10.0f } },
			{ { 46.0f, 26.0f, 1.5f }, {  9.0f, 10.0f, 10.0f } },

			{ { -2.0f, 8.0f,  1.5f }, { 42.0f, 11.0f, 11.0f } },
			{ {-48.0f, 7.0f,  1.5f }, { 15.0f,  9.0f, 12.0f } },
			{ { 46.0f, 7.0f,  1.5f }, { 15.0f,  9.0f, 12.0f } },
			
			{ {  0.0f, 7.0f,-16.0f }, { 24.0f,  8.5f,  9.0f } },
			{ { 40.0f, 7.0f,-16.0f }, { 24.0f,  8.5f,  9.0f } },
			{ {-40.0f, 7.0f,-16.0f }, { 24.0f,  8.5f,  9.0f } },

			{ { 0.0f, 28.0f,-16.0f }, { 60.0f, 12.0f,  9.0f } },

			{ {  0.0f, 7.0f, 19.0f }, { 24.0f,  8.5f,  9.0f } },
			{ { 40.0f, 7.0f, 19.0f }, { 24.0f,  8.5f,  9.0f } },
			{ {-40.0f, 7.0f, 19.0f }, { 24.0f,  8.5f,  9.0f } },

			{ { 0.0f, 28.0f, 19.0f }, { 60.0f, 12.0f,  9.0f } },
			
			{ { 0.0f, 45.0f,  1.0f }, { 60.0f,  8.0f, 10.0f } },
		};

		ReflectionProbe::Settings probeSettings = {};
		m_ReflectionProbeMgr = std::make_unique<ReflectionProbesManager>(GetDevice(), GetMainContext());

		for (uint32 i = 0; i < _countof(probes); i++)
		{
			ReflectionProbe* newProbe = m_ReflectionProbeMgr->Add(GetMainContext(), probeSettings, probes[i].Position, probes[i].Extents);
			newProbe->Bake(GetMainContext(), m_IBLRendering.get(), m_Skybox.get(), &m_LightData, 1, m_RenderObjects.data(), m_RenderObjects.size());
		}
		m_ReflectionProbeMgr->RebuildBuffer(GetMainContext());

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

				m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSSRTex", m_SSR->GetSSRTextureShared());
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
		m_LightPass->Update(GetMainContext(), GetCamera(), &m_LightData, 1, m_CSMRendering.get(), m_ReflectionProbeMgr.get());
	}

	void SponzaDemo::OnRender(const Timer& timer)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		m_CSMRendering->Render(GetMainContext(), m_RenderObjects.data(), m_RenderObjects.size());

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
				UINT SSRMask = 0; // TODO: Temporary workaround for SSR
			} objData;

			XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMMatrixScaling(5, 5, 5)));
			objData.AlbedoTexIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_BASE_COLOR));
			objData.NormalMapIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_NORMAL_MAP));
			objData.MetallicRoughnessIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_METALLIC_ROUGHNESS));
			objData.AOIdx = GetTexIdx(m_Mesh->GetTexture(i, PBR_TEXTURE_AMBIENT_OCCLUSION));
			objData.SSRMask = i == 8; // TODO: Temporary workaround for SSR

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

		m_SSR->Render(GetMainContext(), GetCamera());

		//
		// Post process pass
		//
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		m_PostProcPso.Pso->CommitAll(GetMainContext(), m_PostProcBinder.get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		m_Skybox->Render(GetMainContext(), GetCamera());

		m_GUI.DebugDrawReflectionProbes();
		m_DebugRenderer->Render(GetMainContext(), GetCamera()->GetViewProjMatrix(), GetCamera()->GetPosition());

		m_GUI.RenderImGUI();
	}

	void SponzaDemo::OnResize()
	{
		if (m_GBuffer)
		{
			m_GBuffer->Resize(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);
		}

		if (m_SSR)
		{
			m_SSR->Resize(GetViewport().Width, GetViewport().Height);

			m_GpuCopyDescriptorsSSR = std::move(GetDevice()->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5));

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptorsSSR.GetCpuHandle(0),
				m_GBuffer->GetAccumBuffer(0)->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptorsSSR.GetCpuHandle(1),
				m_GBuffer->GetGBuffer(SponzaGBufferId::Normal)->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptorsSSR.GetCpuHandle(2),
				m_GBuffer->GetGBuffer(SponzaGBufferId::MetalRoughAo)->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuCopyDescriptorsSSR.GetCpuHandle(3),
				GetSwapChain()->GetDepthStencilTextureShared()->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			ScreenSpaceReflection::TextureIndexes texIndexes = {};
			texIndexes.ColorTexIdx = m_GpuCopyDescriptorsSSR.GetGpuHeapIndex(0);
			texIndexes.NormalTexIdx = m_GpuCopyDescriptorsSSR.GetGpuHeapIndex(1);
			texIndexes.MaskTexIdx = m_GpuCopyDescriptorsSSR.GetGpuHeapIndex(2);
			texIndexes.DepthTexIdx = m_GpuCopyDescriptorsSSR.GetGpuHeapIndex(3);

			m_SSR->UpdateIndexes(GetMainContext(), texIndexes);
		}

		if (m_Ssao)
		{
			m_Ssao->Resize(GetViewport().Width, GetViewport().Height);
			m_Ssao->BindResources(m_GBuffer->GetGBufferShared(SponzaGBufferId::Normal), GetSwapChain()->GetDepthStencilTextureShared());

			m_PostProcBinder->DryMutableResources();
			m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_GBuffer->GetAccumBufferShared(0));
			m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSSRTex", m_SSR->GetSSRTextureShared());
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
			deferredLightBuffers.IrradianceMapIdx = m_Skybox->GetIrradianceMap()->GetSRVView()->GetGpuHeapIndex();
			deferredLightBuffers.PrefilteredMapIdx = m_Skybox->GetPrefilteredMap()->GetSRVView()->GetGpuHeapIndex();
			deferredLightBuffers.BRDFLutIdx = m_IBLRendering->GetBrdfLut()->GetSRVView()->GetGpuHeapIndex();

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