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

		m_Ssao = std::make_unique<SSAO>(GetDevice(), GetMainContext(), GetViewport().Width, GetViewport().Height);
		m_Ssao->BindResources(m_NormalRT, GetSwapChain()->GetDepthStencilTextureShared());

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

		DXGI_FORMAT rtvFormats[]
		{
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT_R16G16B16A16_FLOAT
		};

		m_DrawPso.SetDepthStencilState(dss);
		m_DrawPso.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_DrawPso.SetShader(drawVS);
		m_DrawPso.SetShader(drawPS);
		m_DrawPso.SetRTVFormats(2, rtvFormats);
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
			m_DrawBinders[i]->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
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
			XMFLOAT4X4 View;
		};

		ObjData objData;
		PassData passData;
		
		XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMMatrixScaling(20, 20, 20)));

		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		XMStoreFloat4x4(&passData.View, XMMatrixTranspose(XMLoadFloat4x4(&GetCamera()->GetViewMatrix())));
	
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

		GetMainContext()->GetCommandCtx()->TransitionResource(m_ColorRT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_NormalRT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_ColorRT->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_NormalRT->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]
		{
			m_ColorRT->GetRTVView()->GetCpuHandle(),
			m_NormalRT->GetRTVView()->GetCpuHandle(),
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
		
		GetMainContext()->GetCommandCtx()->TransitionResource(m_ColorRT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_NormalRT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

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
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = GetViewport().Width;
		texDesc.Height = GetViewport().Height;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = texDesc.Format;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		D3D12_CLEAR_VALUE clearVal = {};
		clearVal.Color[0] = 0;
		clearVal.Color[1] = 0;
		clearVal.Color[2] = 0;
		clearVal.Color[3] = 1;
		clearVal.Format = texDesc.Format;

		m_ColorRT = std::make_shared<TextureD3D12>(GetDevice(), texDesc, &clearVal, QueueId::Direct);
		m_ColorRT->CreateSRV(&srvDesc);
		m_ColorRT->CreateRTV(&rtvDesc);
		m_ColorRT->SetName(L"m_ColorRT");

		m_NormalRT = std::make_shared<TextureD3D12>(GetDevice(), texDesc, &clearVal, QueueId::Direct);
		m_NormalRT->CreateSRV(&srvDesc);
		m_NormalRT->CreateRTV(&rtvDesc);
		m_NormalRT->SetName(L"m_NormalRT");

		if (m_Ssao)
		{
			m_Ssao->Resize(GetDevice(), GetViewport().Width, GetViewport().Height);
			m_Ssao->BindResources(m_NormalRT, GetSwapChain()->GetDepthStencilTextureShared());

			m_PostProcBinder->DryMutableResources();
			m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_Ssao->GetSSAOMap());
		}
	}
}