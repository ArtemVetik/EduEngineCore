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

		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), "assets\\Models\\scene.gltf");
		m_Mesh->Load(meshDesc);

		m_GBuffer = std::make_unique<GBuffer>(2, G_BUFFERS, 1, ACCUM_BUFFER_FORMAT);
		m_GBuffer->Resize(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);

		m_Ssao = std::make_unique<SSAO>(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);
		m_Ssao->BindResources(m_GBuffer->GetGBufferShared(SponzaGBufferId::Normal), GetSwapChain()->GetDepthStencilTextureShared());

		ShaderResourceDesc sRes[]
		{
			ShaderResourceDesc("cbPerObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(sRes);
		sDesc.ResourceDesc = sRes;

		auto drawVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Color.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto drawPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Color.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);

		auto fsQuadVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto postProcPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PostProc.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);

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

		D3D12_DEPTH_STENCIL_DESC dssOff = {};
		dssOff.DepthEnable = false;

		m_DrawPso.SetDepthStencilState(dss);
		m_DrawPso.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_DrawPso.SetShader(drawVS);
		m_DrawPso.SetShader(drawPS);
		m_DrawPso.SetRTVFormats(SponzaGBufferId::NumBuffers, G_BUFFERS);
		m_DrawPso.Build(GetDevice());

		m_PostProcPso.SetDepthStencilState(dssOff);
		m_PostProcPso.SetShader(fsQuadVS);
		m_PostProcPso.SetShader(postProcPS);
		m_PostProcPso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PostProcPso.Build(GetDevice());

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		m_DrawBinders.resize(m_Mesh->GetMeshCount());
		for (uint32 i = 0; i < m_Mesh->GetMeshCount(); i++)
		{
			m_DrawBinders[i] = m_DrawPso.CreateShaderBinder();

			m_DrawBinders[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
			m_DrawBinders[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
			m_DrawBinders[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gAlbedo", m_Mesh->GetTexture(i)->GetD3D12Texture());
		}

		m_PostProcBinder = m_PostProcPso.CreateShaderBinder();
		m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_Ssao->GetSSAOMap());
	}
	
	void SponzaDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera());

		struct ObjData
		{
			XMFLOAT4X4 World;
		};

		struct PassData
		{
			XMFLOAT4X4 ViewProj;
		};

		ObjData objData;
		PassData passData;
		
		XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMMatrixScaling(20, 20, 20)));
		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
	
		m_ObjBuffer->LoadData(GetMainContext(), objData);
		m_PassBuffer->LoadData(GetMainContext(), passData);

		m_Ssao->Update(GetCamera(), GetMainContext());
	}

	void SponzaDemo::OnRender(const Timer& timer)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(0), D3D12_RESOURCE_STATE_RENDER_TARGET);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(1), D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_GBuffer->GetGBufferRTVView(0), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_GBuffer->GetGBufferRTVView(1), clear, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]
		{
			m_GBuffer->GetGBufferRTVView(0),
			m_GBuffer->GetGBufferRTVView(1),
		};

		GetMainContext()->GetCommandCtx()->SetRenderTargets(2, rtvs, false, &GetSwapChain()->DepthStencilView());

		for (uint32 i = 0; i < m_Mesh->GetMeshCount(); i++)
		{
			m_DrawPso.CommitAll(GetMainContext(), m_DrawBinders[i].get());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_Mesh->GetIndexBuffer(i)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_Mesh->GetVertexBuffer(i)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexCount(i), 1, 0, 0, 0);
		}
		
		GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(1), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		m_Ssao->Render(GetMainContext());

		//
		// Post process pass
		//
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		m_PostProcPso.CommitAll(GetMainContext(), m_PostProcBinder.get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

	}

	void SponzaDemo::OnResize()
	{
		if (m_GBuffer)
		{
			m_GBuffer->Resize(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);
		}

		if (m_Ssao)
		{
			m_Ssao->Resize(GetDevice(), GetViewport().Width, GetViewport().Height);
			m_Ssao->BindResources(m_GBuffer->GetGBufferShared(SponzaGBufferId::Normal), GetSwapChain()->GetDepthStencilTextureShared());

			m_PostProcBinder->DryMutableResources();
			m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_Ssao->GetSSAOMap());
		}
	}
}