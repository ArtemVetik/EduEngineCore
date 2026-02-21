#include "TemporalAADemo.h"

#include <DemoHelpers.h>

namespace EduEngine
{

	static constexpr char* Models[]
	{
		"assets\\Models\\tree_gn\\scene.gltf",
		"assets\\Models\\stylised_sky_player_home_dioroma\\scene.gltf",
	};

	static constexpr char* Textures[]
	{
		"assets\\Textures\\tree_gn\\",
		"assets\\Textures\\stylised_sky_player_home_dioroma\\",
	};

	__forceinline float RadicalInverseBase2(uint32 bits)
	{
		bits = (bits << 16u) | (bits >> 16u);
		bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
		bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
		bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
		bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
		return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
	}

	__forceinline XMFLOAT2 Hammersley2D(uint64 sampleIdx, uint64 numSamples)
	{
		return XMFLOAT2(float(sampleIdx) / float(numSamples), RadicalInverseBase2(uint32(sampleIdx)));
	}

	void TemporalAADemo::OnStartUp()
	{
		GetCamera()->Setup(
			{ 0, 25, -100 },
			{ 0, 0, 1 },
			{ 1, 0, 0 },
			{ 1, 1, 0 }
		);

		TextureLoadDesc texLoadDesc = {};
		texLoadDesc.Flags = TextureLoadDesc::CREATE_SRV;

		Texture skyTex(GetDevice(), texLoadDesc);
		skyTex.Load(L"assets\\Textures\\cubemap.dds", GetMainContext());

		ShaderResourceDesc sRes[]
		{
			ShaderResourceDesc("cbPerObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(sRes);
		sDesc.ResourceDesc = sRes;

		auto drawVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Draw.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto drawPS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Draw.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);

		auto fsQuadVS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);

		auto resolvePS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Resolve.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);
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

		DXGI_FORMAT rtvFormats[]
		{
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT_R16G16_FLOAT
		};

		m_DrawPso.SetRTVFormats(2, rtvFormats);
		m_DrawPso.Build(GetDevice());

		m_ResolvePso.SetDepthStencilState(dssOff);
		m_ResolvePso.SetShader(fsQuadVS);
		m_ResolvePso.SetShader(resolvePS);
		m_ResolvePso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_ResolvePso.Build(GetDevice());

		m_PostProcPso.SetDepthStencilState(dssOff);
		m_PostProcPso.SetShader(fsQuadVS);
		m_PostProcPso.SetShader(postProcPS);
		m_PostProcPso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PostProcPso.Build(GetDevice());

		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		auto vs_Skybox = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\SkyboxTAA.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto ps_Skybox = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\SkyboxTAA.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);

		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		m_SkyboxPso.SetDepthStencilState(dss);
		m_SkyboxPso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		m_SkyboxPso.SetShader(vs_Skybox);
		m_SkyboxPso.SetShader(ps_Skybox);
		m_SkyboxPso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_SkyboxPso.Build(GetDevice());
		m_SkyboxPso.SetName(L"PSO_Skybox");

		m_SkyboxPassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		m_SkyboxBinder = m_SkyboxPso.CreateShaderBinder();
		m_SkyboxBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_SkyboxPassBuffer);
		m_SkyboxBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_SkyboxPassBuffer);
		m_SkyboxBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gCubeMap", skyTex.GetD3D12Texture());

		for (uint8 i = 0; i < 2; i++)
		{
			m_ResolveBinder[i] = m_ResolvePso.CreateShaderBinder();
			m_PostProcBinder[i] = m_PostProcPso.CreateShaderBinder();
		}

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		LoadModel(Models[m_ModelIdx], Textures[m_ModelIdx]);

		DirectX::XMFLOAT3 vertices[] =
		{
			// +X
			{ +1, +1, -1 }, { +1, -1, -1 }, { +1, -1, +1 }, { +1, +1, +1 },
			// -X
			{ -1, +1, +1 }, { -1, -1, +1 }, { -1, -1, -1 }, { -1, +1, -1 },
			// +Y
			{ -1, +1, +1 }, { -1, +1, -1 }, { +1, +1, -1 }, { +1, +1, +1 },
			// -Y
			{ -1, -1, -1 }, { -1, -1, +1 }, { +1, -1, +1 }, { +1, -1, -1 },
			// +Z
			{ -1, +1, +1 }, { +1, +1, +1 }, { +1, -1, +1 }, { -1, -1, +1 },
			// -Z
			{ +1, +1, -1 }, { -1, +1, -1 }, { -1, -1, -1 }, { +1, -1, -1 },
		};

		uint16 indices[] = {
			0,1,2,		0,2,3,		// +X
			4,5,6,		4,6,7,      // -X
			8,9,10,		8,10,11,	// +Y
			12,13,14,	12,14,15,	// -Y
			16,17,18,	16,18,19,	// +Z
			20,21,22,	20,22,23,	// -Z
		};

		m_CubeVB = std::make_shared<VertexBufferD3D12>(GetDevice(), GetMainContext(), vertices, sizeof(DirectX::XMFLOAT3), _countof(vertices));
		m_CubeIB = std::make_shared<IndexBufferD3D12>(GetDevice(), GetMainContext(), indices, sizeof(uint16), _countof(indices), DXGI_FORMAT_R16_UINT);

		m_CubeVB->SetName(L"VB Cube");
		m_CubeIB->SetName(L"IB Cube");

		m_DebugRenderer = std::make_shared<DebugRendererSystem>(GetDevice());

		OnResize();
	}

	void TemporalAADemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera());

		static uint64 frameCount = 0;
		const float jitterScale = 1.0f;

		uint64 idx = frameCount % 8;
		XMFLOAT2 jitter = Hammersley2D(idx, 8);

		jitter.x = jitter.x * 2 - 1;
		jitter.y = jitter.y * 2 - 1;

		jitter.x *= jitterScale;
		jitter.y *= jitterScale;

		const float offsetX = jitter.x * (1.0f / GetViewport().Width);
		const float offsetY = jitter.y * (1.0f / GetViewport().Height);
		XMMATRIX offsetMatrix = XMMatrixTranslation(offsetX, -offsetY, 0.0f);

		struct ObjData
		{
			XMFLOAT4X4 CurrWorld;
			XMFLOAT4X4 PrevWorld;
		};

		struct PassData
		{
			XMFLOAT4X4 ViewProj;
			XMFLOAT4X4 CurrViewProjNoJitter;
			XMFLOAT4X4 PrevViewProjNoJitter;
			XMFLOAT2 RTSize;
			XMFLOAT2 Jitter;
		};

		static ObjData objData;
		static PassData passData;

		objData.PrevWorld = objData.CurrWorld;
		passData.PrevViewProjNoJitter = passData.CurrViewProjNoJitter;

		XMMATRIX RT = XMMatrixIdentity();

		if (m_Animate)
		{
			RT = XMMatrixRotationY(m_AnimateSpeed * timer.GetTotalTime()) *
				XMMatrixTranslation(sin(m_AnimateSpeed * timer.GetTotalTime()) * 50, 0, 0);
		}

		XMStoreFloat4x4(&objData.CurrWorld, XMMatrixTranspose(XMMatrixScaling(3, 3, 3) * RT));
		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix() * offsetMatrix));
		XMStoreFloat4x4(&passData.CurrViewProjNoJitter, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passData.RTSize = XMFLOAT2(GetViewport().Width, GetViewport().Height);
		passData.Jitter = XMFLOAT2(-jitter.x, jitter.y);

		m_ObjBuffer->LoadData(GetMainContext(), objData);
		m_PassBuffer->LoadData(GetMainContext(), passData);

		frameCount++;
	}

	void TemporalAADemo::OnRender(const Timer& timer)
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

		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_CurrentTex->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_MotionVectors->GetRTVView()->GetCpuHandle(), clear, 0, nullptr);

		
		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]
		{
			m_CurrentTex->GetRTVView()->GetCpuHandle(),
			m_MotionVectors->GetRTVView()->GetCpuHandle(),
		};

		//
		// Render geometry and generate velocity texture
		//
		GetMainContext()->GetCommandCtx()->SetRenderTargets(2, rtvs, false, &GetSwapChain()->DepthStencilView());

		for (uint32 i = 0; i < m_Mesh->GetMeshCount(); i++)
		{
			m_DrawPso.CommitAll(GetMainContext(), m_DrawBinders[i].get());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_Mesh->GetIndexBuffer(i)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_Mesh->GetVertexBuffer(i)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexCount(i), 1, 0, 0, 0);
		}

		//
		// Render skybox
		//
		{
			struct SkyboxPassCB
			{
				XMFLOAT4X4 View;
				XMFLOAT4X4 Proj;
				float Lod;
				XMUINT3 Padding;
			};

			SkyboxPassCB cb = {};
			XMStoreFloat4x4(&cb.View, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&GetCamera()->GetViewMatrix())));
			XMStoreFloat4x4(&cb.Proj, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&GetCamera()->GetProjectionMatrix())));
			cb.Lod = 0;

			m_SkyboxPassBuffer->LoadData(GetMainContext(), cb);

			m_SkyboxPso.CommitAll(GetMainContext(), m_SkyboxBinder.get());

			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_CubeVB->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_CubeIB->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_CubeIB->GetLength(), 1, 0, 0, 0);
		}

		static uint8 pingPong = 0;

		GetMainContext()->GetCommandCtx()->TransitionResource(m_HistoryTex[pingPong].get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_HistoryTex[1 - pingPong].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		//
		// Resolve pass
		//
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &m_HistoryTex[pingPong]->GetRTVView()->GetCpuHandle(), true, nullptr);
		m_ResolvePso.CommitAll(GetMainContext(), m_ResolveBinder[1 - pingPong].get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		GetMainContext()->GetCommandCtx()->TransitionResource(m_HistoryTex[1 - pingPong].get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_HistoryTex[pingPong].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		//
		// Post process pass
		//
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		m_PostProcPso.CommitAll(GetMainContext(), m_PostProcBinder[pingPong].get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		pingPong = 1 - pingPong;

		//
		// Render GUI
		//
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_Always);
		ImGui::SetNextWindowSize({ 350, 140 }, ImGuiCond_Always);
		ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		const char* models[] = {
			"Tree",
			"SkyHome",
		};

		const char* velModes[] = {
			"Simple",
			"Greatest Velocity",
		};

		if (ImGui::Combo("Model", (int*)&m_ModelIdx, models, IM_ARRAYSIZE(models)))
		{
			LoadModel(Models[m_ModelIdx], Textures[m_ModelIdx]);
		}

		if (ImGui::Combo("Velocity Sample", (int*)&m_VelocityMode, velModes, IM_ARRAYSIZE(velModes)))
		{
			BuildResolveConstantBuffer();
		}

		ImGui::Checkbox("Animate", &m_Animate);

		if (m_Animate)
		{
			ImGui::DragFloat("Speed", &m_AnimateSpeed, 0.1f, -5.0f, 5.0f);
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void TemporalAADemo::OnResize()
	{
		if (!m_ResolveBinder[0])
			return;

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

		m_CurrentTex = std::make_shared<TextureD3D12>(GetDevice(), texDesc, &clearVal, QueueId::Direct);
		m_CurrentTex->CreateSRV(&srvDesc);
		m_CurrentTex->CreateRTV(&rtvDesc);
		m_CurrentTex->SetName(L"m_CurrentTex");

		BuildResolveConstantBuffer();

		for (uint8 i = 0; i < 2; i++)
		{
			m_HistoryTex[i] = std::make_shared<TextureD3D12>(GetDevice(), texDesc, &clearVal, QueueId::Direct);
			m_HistoryTex[i]->CreateSRV(&srvDesc);
			m_HistoryTex[i]->CreateRTV(&rtvDesc);

			wchar_t bufferName[16];
			swprintf(bufferName, 16, L"m_HistoryTex-%d", i);
			m_HistoryTex[i]->SetName(bufferName);

			m_ResolveBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gCurrentTex", m_CurrentTex);
			m_ResolveBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gHistoryTex", m_HistoryTex[i]);
			m_ResolveBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_ResolveConstantsBuffer);

			m_PostProcBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gSceneTex", m_HistoryTex[i]);
		}

		//
		// Create and bind motion vector texture
		//

		texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		rtvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		clearVal.Format = DXGI_FORMAT_R16G16_FLOAT;
		m_MotionVectors = std::make_shared<TextureD3D12>(GetDevice(), texDesc, &clearVal, QueueId::Direct);
		m_MotionVectors->CreateSRV(&srvDesc);
		m_MotionVectors->CreateRTV(&rtvDesc);

		for (uint8 i = 0; i < 2; i++)
		{
			m_ResolveBinder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gVelocityTex", m_MotionVectors);
		}

		GetMainContext()->GetCommandCtx()->TransitionResource(m_CurrentTex.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		GetMainContext()->GetCommandCtx()->TransitionResource(m_MotionVectors.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void TemporalAADemo::LoadModel(const char* model, const char* texturePath)
	{
		MeshLoadDesc meshDesc = {};
		meshDesc.Flags = MESH_LOAD_FLAG_LOAD_TEXTURES;
		meshDesc.TextureBasePath = texturePath;
		meshDesc.TextureExt = ".dds";

		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), model);
		m_Mesh->Load(meshDesc);

		m_DrawBinders.resize(m_Mesh->GetMeshCount());

		for (uint32 i = 0; i < m_Mesh->GetMeshCount(); i++)
		{
			m_DrawBinders[i] = m_DrawPso.CreateShaderBinder();
			m_DrawBinders[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
			m_DrawBinders[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
			m_DrawBinders[i]->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);

			if (m_Mesh->GetTexture(i))
				m_DrawBinders[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gAlbedo", m_Mesh->GetTexture(i)->GetD3D12Texture());
		}
	}

	void TemporalAADemo::BuildResolveConstantBuffer()
	{
		struct ResolveConstantsData
		{
			XMFLOAT2 TextureSize;
			UINT VelocityMode;
			UINT Padding;
		};

		ResolveConstantsData data;
		data.TextureSize = XMFLOAT2(GetViewport().Width, GetViewport().Height);
		data.VelocityMode = m_VelocityMode;

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.Width = sizeof(ResolveConstantsData);
		buffDesc.Height = 1;
		buffDesc.Alignment = 0;
		buffDesc.MipLevels = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		m_ResolveConstantsBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, &data, QueueId::Direct);
	}
}