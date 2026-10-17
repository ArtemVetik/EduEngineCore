#include "VirtualShadowMapsDemo.h"

#include "../../InputSystem/include/InputManager.h"
#include "../../ShaderBinding/EduBinding/include/PipelineState.h"

#include <FileUtils.h>
#include <DemoHelpers.h>
#include <ShaderCache.h>

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
	{
		UINT AlbedoTex;
		UINT NormalTex;
		UINT AccumTex;
		UINT ShadowMask;
	};

	void VirtualShadowMapsDemo::OnStartUp()
	{
		//
		// Loading mesh
		//
		MeshLoadDesc meshDesc = {};
		meshDesc.VertexInput = MESH_LOAD_VERTEX_INPUT_POSITION | MESH_LOAD_VERTEX_INPUT_NORMAL | MESH_LOAD_VERTEX_INPUT_COLOR;

		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), "assets\\Models\\vsm_test_scene.glb");
		m_Mesh->Load(meshDesc);

		//
		// Creating GPU buffers
		//
		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantsData));
		m_ConstantsBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), desc, QueueId::Direct);

		DXGI_FORMAT gBufFormats[2]
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R16G16B16A16_FLOAT
		};

		m_GBuffer = std::make_unique<GBuffer>(2, gBufFormats, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

		//
		// Creating PSO
		//
		ShaderResourceDesc resDesc[]
		{ 
			ShaderResourceDesc("cbPerObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC)
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceDesc = resDesc;
		sDesc.ResourceNum = _countof(resDesc);

		auto vsGeometry = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\Color.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto psGeometry = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\Color.hlsl", L"PS_Geometry", L"ps_6_6", nullptr, sDesc);
		auto vsFsQuad = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto psLight = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\Color.hlsl", L"PS_Lighting", L"ps_6_6", nullptr, sDesc);
		auto psPostProc = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\Color.hlsl", L"PS_PostProc", L"ps_6_6", nullptr, sDesc);
		
		D3D12_INPUT_ELEMENT_DESC inputLayout[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		m_GeometryPso.SetInputLayout({ inputLayout, _countof(inputLayout)});
		m_GeometryPso.SetShader(vsGeometry);
		m_GeometryPso.SetShader(psGeometry);
		m_GeometryPso.SetRTVFormats(2, gBufFormats);
		m_GeometryPso.SetDepthStencilState(dss);
		m_GeometryPso.Build(GetDevice());
		m_GeometryPso.SetName(L"GeometryPSO");

		m_GeometryBinder = m_GeometryPso.CreateShaderBinder();
		m_GeometryBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		m_GeometryBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);

		dss.DepthEnable = false;

		m_LightPso.SetShader(vsFsQuad);
		m_LightPso.SetShader(psLight);
		m_LightPso.SetDepthStencilState(dss);
		m_LightPso.SetRTVFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		m_LightPso.Build(GetDevice());

		m_LightBinder = m_LightPso.CreateShaderBinder();
		m_LightBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		m_LightBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbBuffers", m_ConstantsBuffer);

		m_PostProcPso.SetShader(vsFsQuad);
		m_PostProcPso.SetShader(psPostProc);
		m_PostProcPso.SetDepthStencilState(dss);
		m_PostProcPso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PostProcPso.Build(GetDevice());

		m_PostProcBinder = m_PostProcPso.CreateShaderBinder();
		m_PostProcBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbBuffers", m_ConstantsBuffer);

		//
		// Setup VSM
		//

		m_RenderObjects[0].Mesh = m_Mesh.get();
		m_RenderObjects[0].World = XMMatrixScaling(5, 5, 5);

		m_VSM = std::make_unique<VirtualShadowMap>(GetDevice(), GetMainContext());
		m_VSM->BuildCommandSignature(GetDevice(), GetMainContext(), m_RenderObjects, 1);

		OnResize();
	}

	void VirtualShadowMapsDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera(), 50.0f);
		
		struct PassData
		{
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 LightDirection;
		} passData;

		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passData.LightDirection = m_Light.Direction;

		m_PassBuffer->LoadData(GetMainContext(), passData);

		m_VSM->Update(GetMainContext(), GetCamera(), &m_Light);
	}

	void VirtualShadowMapsDemo::OnRender(const Timer& timer)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		CommandContext* ctx = GetMainContext()->GetCommandCtx();

		ctx->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		ctx->FlushResourceBarriers();
		ctx->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		ctx->SetViewports(&GetViewport(), 1);
		ctx->SetScissorRects(&GetScissorRect(), 1);

		const float clear[4] = { 0, 0, 0, 1 };

		for (uint32 i = 0; i < 2; i++)
			GetMainContext()->GetCommandCtx()->TransitionResource(m_GBuffer->GetGBuffer(i), D3D12_RESOURCE_STATE_RENDER_TARGET);

		ctx->TransitionResource(m_GBuffer->GetAccumBuffer(0), D3D12_RESOURCE_STATE_RENDER_TARGET);
		ctx->FlushResourceBarriers();

		for (uint32 i = 0; i < 2; i++)
			ctx->GetCmdList()->ClearRenderTargetView(m_GBuffer->GetGBufferRTVView(i), clear, 0, nullptr);

		ctx->GetCmdList()->ClearRenderTargetView(m_GBuffer->GetAccumBuffer(0)->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);

		ctx->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		// Geometry Pass
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvViews[2]
			{
				m_GBuffer->GetGBufferRTVView(0),
				m_GBuffer->GetGBufferRTVView(1)
			};

			ctx->SetRenderTargets(2, rtvViews, false, &GetSwapChain()->DepthStencilView());

			m_GeometryPso.BeginPSO(GetMainContext());

			int count = _countof(m_RenderObjects);
			for (uint32 obj = 0; obj < count; obj++)
			{
				struct ObjData
				{
					XMFLOAT4X4 World;
				} objData;

				XMStoreFloat4x4(&objData.World, XMMatrixTranspose(m_RenderObjects[obj].World));
				m_ObjBuffer->LoadData(GetMainContext(), objData);

				m_GeometryPso.CommitResources(GetMainContext(), m_GeometryBinder.get());

				for (uint32 mIdx = 0; mIdx < m_RenderObjects[obj].Mesh->GetMeshCount(); mIdx++)
				{
					ctx->GetCmdList()->IASetVertexBuffers(0, 1, &m_Mesh->GetVertexBuffer(mIdx)->GetView());
					ctx->GetCmdList()->IASetIndexBuffer(&m_Mesh->GetIndexBuffer(mIdx)->GetView());
					ctx->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexCount(mIdx), 1, 0, 0, 0);
				}
			}
		}

		for (uint32 i = 0; i < 2; i++)
			ctx->TransitionResource(m_GBuffer->GetGBuffer(i), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		ctx->TransitionResource(GetSwapChain()->GetDepthStencilTexture(), 
			D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		
		ctx->FlushResourceBarriers();

		m_VSM->Compute(GetMainContext(), m_RenderObjects, 1);

		ctx->SetViewports(&GetViewport(), 1);
		ctx->SetScissorRects(&GetScissorRect(), 1);

		// Lighting Pass
		{
			ctx->SetRenderTargets(1, &m_GBuffer->GetAccumBuffRTVView(0), false, nullptr);

			m_LightPso.BeginPSOAndCommitResources(GetMainContext(), m_LightBinder.get());
			ctx->GetCmdList()->DrawInstanced(3, 1, 0, 0);
		}

		ctx->TransitionResource(m_GBuffer->GetAccumBuffer(0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		ctx->FlushResourceBarriers();
		
		// Post Proc
		{
			ctx->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), false, nullptr);
			ctx->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);


			m_PostProcPso.BeginPSOAndCommitResources(GetMainContext(), m_PostProcBinder.get());
			ctx->GetCmdList()->DrawInstanced(3, 1, 0, 0);
		}

		ctx->TransitionResource(GetSwapChain()->GetDepthStencilTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	void VirtualShadowMapsDemo::OnResize()
	{
		XMUINT2 screenSize = { (UINT)GetSwapChain()->GetWidth(), (UINT)GetSwapChain()->GetHeight() };

		if (m_GBuffer)
		{
			m_GBuffer->Resize(GetDevice(), GetMainContext(), screenSize.x, screenSize.y);
		}

		if (m_VSM)
		{
			m_GpuSSDepth = std::move(GetDevice()->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1));
			GetDevice()->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuSSDepth.GetCpuHandle(), GetSwapChain()->GetDepthStencilTexture()->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			m_VSM->SetSSDepth(GetDevice(), GetMainContext(), m_GpuSSDepth.GetGpuHeapIndex(), screenSize);
		}

		if (m_GBuffer && m_VSM)
		{
			ConstantsData constData = {};
			constData.ShadowMask = m_VSM->GetShadowMask()->GetSRVView()->GetGpuHeapIndex();
			constData.AlbedoTex = m_GBuffer->GetGBuffer(0)->GetSRVView()->GetGpuHeapIndex();
			constData.NormalTex = m_GBuffer->GetGBuffer(1)->GetSRVView()->GetGpuHeapIndex();
			constData.AccumTex = m_GBuffer->GetAccumBuffer(0)->GetSRVView()->GetGpuHeapIndex();

			m_ConstantsBuffer->LoadData(GetMainContext(), &constData);
		}
	}
}