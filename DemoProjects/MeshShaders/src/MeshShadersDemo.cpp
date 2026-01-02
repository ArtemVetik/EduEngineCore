#include "MeshShadersDemo.h"

#include <InputManager.h>
#include <fstream>

namespace EduEngine
{
	void MeshShadersDemo::OnStartUp()
	{
		MeshLoadDesc loadDesc = { };
		loadDesc.CreateVertexSRV = true;
		
		m_Model = std::make_unique<Mesh>(GetDevice(), GetMainContext(), "assets\\models\\Dragon.obj");
		m_Model->Load(&loadDesc);

		struct FileHeader
		{
			uint32_t prolog;
			uint32_t version;
			uint32_t meshlets_count;
			uint32_t vertices_count;
			uint32_t triangles_count;
		};

		FileHeader fh = {};

		std::ifstream istream("assets\\models\\Dragon.mshl", std::ios::binary);

		istream.read(reinterpret_cast<char*>(&fh), sizeof(FileHeader));

		if (fh.prolog != 'LHSM')
		{
			ASSERT_FAILED("Expected file prolog: ", "MSHL");
			return;
		}

		std::vector<Meshlet> meshlets(fh.meshlets_count);
		std::vector<uint32_t> meshlet_vertices(fh.vertices_count);
		std::vector<uint32_t> meshlet_triangles_packed(fh.triangles_count);

		m_MeshletCount = fh.meshlets_count;
		istream.read(reinterpret_cast<char*>(meshlets.data()), sizeof(Meshlet) * fh.meshlets_count);
		istream.read(reinterpret_cast<char*>(meshlet_vertices.data()), sizeof(unsigned int) * fh.vertices_count);
		istream.read(reinterpret_cast<char*>(meshlet_triangles_packed.data()), sizeof(unsigned int) * fh.triangles_count);

		istream.close();

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(Meshlet) * fh.meshlets_count;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = fh.meshlets_count;
		srvDesc.Buffer.StructureByteStride = sizeof(Meshlet);
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;

		m_Meshlet = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), desc, QueueId::Direct);
		m_Meshlet->LoadData(GetMainContext(), meshlets.data());
		m_Meshlet->CreateSRV(&srvDesc);
		m_Meshlet->SetName(L"m_Meshlet");

		desc.Width = sizeof(uint32_t) * fh.vertices_count;
		srvDesc.Buffer.NumElements = fh.vertices_count;
		srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);

		m_MeshletVertices = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), desc, QueueId::Direct);
		m_MeshletVertices->LoadData(GetMainContext(), meshlet_vertices.data());
		m_MeshletVertices->CreateSRV(&srvDesc);
		m_MeshletVertices->SetName(L"m_MeshletVertices");

		desc.Width = sizeof(uint32_t) * fh.triangles_count;
		srvDesc.Buffer.NumElements = fh.triangles_count;
		srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);

		m_MeshletTris = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), desc, QueueId::Direct);
		m_MeshletTris->LoadData(GetMainContext(), meshlet_triangles_packed.data());
		m_MeshletTris->CreateSRV(&srvDesc);
		m_MeshletTris->SetName(L"m_MeshletTris");

		D3D12_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = true;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		ShaderResourceDesc res[]{ ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC) };

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = 1;
		sDesc.ResourceDesc = res;

		auto ms = std::make_shared<ShaderD3D12>(L"assets\\shaders\\DrawMeshlet.hlsl", L"MS", L"ms_6_5", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\shaders\\DrawMeshlet.hlsl", L"PS", L"ps_6_5", nullptr, sDesc);

		m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_Pso.SetShader(ms);
		m_Pso.SetShader(ps);
		m_Pso.SetDepthStencilState(dsDesc);
		m_Pso.Build(GetDevice());

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		m_Binder = m_Pso.CreateShaderBinder();
		m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "Vertices", m_Model->GetVertexBufferShared());
		m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "Meshlets", m_Meshlet);
		m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "MeshletVertices", m_MeshletVertices);
		m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "MeshletIndices", m_MeshletTris);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_MESH, "cbPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
	}

	void MeshShadersDemo::OnUpdate(const Timer& timer)
	{
		XMVECTOR direction = XMLoadFloat3(&GetCamera()->GetLook());
		XMVECTOR lrVector = XMLoadFloat3(&GetCamera()->GetRight());
		XMVECTOR upVector = XMLoadFloat3(&GetCamera()->GetUp());

		float moveScale = 150.0f;
		static constexpr float rotateScale = 0.01f;
		static constexpr float rotateLerpSpeed = 20.0f;

		if (InputManager::GetInstance().IsKeyPressed(DIK_LSHIFT))
			moveScale *= 2;

		if (InputManager::GetInstance().IsKeyPressed(DIK_W))
			GetCamera()->Move(direction * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_S))
			GetCamera()->Move(-direction * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_A))
			GetCamera()->Move(-lrVector * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_D))
			GetCamera()->Move(lrVector * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_E))
			GetCamera()->Move(upVector * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_Q))
			GetCamera()->Move(-upVector * moveScale * timer.GetDeltaTime());

		auto mouseState = InputManager::GetInstance().GetMouseState();

		static XMFLOAT2 currentDelta = { 0, 0 };
		static XMFLOAT2 targetDelta = { 0, 0 };

		if ((mouseState.rgbButtons[1] & 0x80) != 0)
		{
			targetDelta.x += mouseState.lX * rotateScale;
			targetDelta.y += mouseState.lY * rotateScale;
		}

		auto Lerp = [](float a, float b, float t) {
			return a + (b - a) * t;
			};

		float prevX = currentDelta.x;
		currentDelta.x = Lerp(currentDelta.x, targetDelta.x, timer.GetDeltaTime() * rotateLerpSpeed);
		GetCamera()->RotateY(currentDelta.x - prevX);

		float prevY = currentDelta.y;
		currentDelta.y = Lerp(currentDelta.y, targetDelta.y, timer.GetDeltaTime() * rotateLerpSpeed);
		GetCamera()->Pitch(currentDelta.y - prevY);

		GetCamera()->Update(timer);
	}

	void MeshShadersDemo::OnRender(const Timer& timer)
	{
		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		ID3D12DescriptorHeap* heaps[]{ GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(heaps), heaps);

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		struct Pass
		{
			XMFLOAT4X4 World;
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CameraPos;
			UINT Padding = 0;
		} pass;

		XMStoreFloat4x4(&pass.World, XMMatrixTranspose(XMMatrixScaling(500, 500, 500)));
		XMStoreFloat4x4(&pass.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		pass.CameraPos = GetCamera()->GetPosition();

		m_PassBuffer->LoadData(GetMainContext(), pass);

		m_Pso.CommitAll(GetMainContext(), m_Binder.get());

		static_cast<ID3D12GraphicsCommandList6*>(GetMainContext()->GetCommandCtx()->GetCmdList())->DispatchMesh(m_MeshletCount, 1, 1);
	}
}