#include "MeshShadersDemo.h"

#include <InputManager.h>
#include <fstream>

namespace EduEngine
{
	static const char* BaseModelPath = "assets\\Models\\bunny_LOD";
	static const uint32 MaxDispatchGroups = 65536u;

	struct CullData
	{
		float sphere_center[3];
		float radius;
		float cone_apex[3];
		signed char cone_axis_s8[3];
		signed char cone_cutoff_s8;
	};

	struct DispatchData
	{
		UINT DispatchInstanceOffset;
		UINT DispatchInstanceCount;
		XMUINT2 Padding;
	};

	struct PassData
	{
		XMFLOAT4X4 ViewProj;

		XMFLOAT3 CameraPos;
		UINT InstanceCount;

		float InvTanHalfFovY;
		UINT LodCount;
		UINT RenderMode;
		UINT Padding;

		XMFLOAT4 Planes[6];
	};

	void MeshShadersDemo::OnStartUp()
	{
		m_LodCount = 4;
		m_GridSize = XMUINT3{ 1, 1, 1 };

		XMUINT4 meshletInfoPacked[MAX_LOD_LEVEL];
		memset(meshletInfoPacked, 0, sizeof(meshletInfoPacked));

		auto CreateBuffer = [&](std::shared_ptr<BufferD3D12>& buffer,
								bool createSrv,
								uint32 numElements,
								uint32 byteStride,
								void* loadData,
								const wchar_t* name)
			{
				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Alignment = 0;
				desc.Width = byteStride * numElements;
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
				srvDesc.Buffer.NumElements = numElements;
				srvDesc.Buffer.StructureByteStride = byteStride;
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;

				buffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), desc, QueueId::Direct);
				buffer->LoadData(GetMainContext(), loadData);
				if (createSrv)
					buffer->CreateSRV(&srvDesc);
				buffer->SetName(name);
			};

		for (size_t i = 0; i < m_LodCount; i++)
		{
			std::string modelPath = BaseModelPath + std::to_string(i) + ".obj";
			std::string meshletPath = BaseModelPath + std::to_string(i) + ".mshl";

			MeshLoadDesc loadDesc = { };
			loadDesc.Flags = MESH_LOAD_FLAG_CREATE_VERTEX_SRV | MESH_LOAD_FLAG_GEN_BOUNDING_BOX;

			m_Model[i] = std::make_unique<Mesh>(GetDevice(), GetMainContext(), modelPath.c_str());
			m_Model[i]->Load(loadDesc);

			std::vector<Meshlet> meshlets;
			std::vector<CullData> cull_data;
			std::vector<uint32_t> meshlet_vertices;
			std::vector<uint32_t> meshlet_triangles_packed;

			ReadMeshlet(meshletPath.c_str(), meshlets, cull_data, meshlet_vertices, meshlet_triangles_packed);

			CreateBuffer(m_Meshlet[i], true, meshlets.size(), sizeof(Meshlet), meshlets.data(), L"m_Meshlet");
			CreateBuffer(m_MeshletVertices[i], true, meshlet_vertices.size(), sizeof(uint32_t), meshlet_vertices.data(), L"m_MeshletVertices");
			CreateBuffer(m_MeshletTris[i], true, meshlet_triangles_packed.size(), sizeof(uint32_t), meshlet_triangles_packed.data(), L"m_MeshletTris");

			meshletInfoPacked[i] = XMUINT4(meshlets.size(), meshlets[meshlets.size() - 1].VertexCount, meshlets[meshlets.size() - 1].TriangleCount, 0);
		}

		CreateBuffer(m_MeshletData, false, 1, sizeof(meshletInfoPacked), meshletInfoPacked, L"m_MeshletData");

		BuildInstanceBuffer();

		//
		//	Initialize PSO
		//

		ShaderResourceDesc res[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbDispatchData", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = { };
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(res);
		sDesc.ResourceDesc = res;

		auto as = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\DrawMeshlet.hlsl", L"AS", L"as_6_5", nullptr, sDesc);
		auto ms = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\DrawMeshlet.hlsl", L"MS", L"ms_6_5", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\DrawMeshlet.hlsl", L"PS", L"ps_6_5", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = true;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_Pso.SetShader(as);
		m_Pso.SetShader(ms);
		m_Pso.SetShader(ps);
		m_Pso.SetDepthStencilState(dsDesc);
		m_Pso.Build(GetDevice());

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_DispatchDataBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		m_Binder = m_Pso.CreateShaderBinder();

		BindResources();
	}

	void MeshShadersDemo::OnUpdate(const Timer& timer)
	{
		XMVECTOR direction = XMLoadFloat3(&GetCamera()->GetLook());
		XMVECTOR lrVector = XMLoadFloat3(&GetCamera()->GetRight());
		XMVECTOR upVector = XMLoadFloat3(&GetCamera()->GetUp());

		float moveScale = 100.0f;
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

		//
		// Update constant buffers data
		//

		PassData passData = {};
		XMMATRIX viewProjT = XMMatrixTranspose(GetCamera()->GetViewProjMatrix());
		XMStoreFloat4x4(&passData.ViewProj, viewProjT);
		passData.CameraPos = GetCamera()->GetPosition();
		passData.InstanceCount = m_InstanceCount;
		passData.LodCount = m_LodCount;
		passData.RenderMode = m_RenderMode;
		passData.InvTanHalfFovY = 1.0f / tanf(GetCamera()->GetFovY() * 0.5f);

		// -w <= x <= w | -> | L: (x >= -w), (x + w >= 0) | R: (x <= w), (w - x >= 0)
		// -w <= y <= w | -> | B: (y >= -w), (y + w >= 0) | U: (y <= w), (w - y >= 0)
		//  0 <= z <= w | -> | N: (z >= 0)				  | F: (z <= w), (w - z >= 0)
		//
		// x = r[0], y = r[1], z = r[2], w = r[3]
		XMStoreFloat4(&passData.Planes[0], XMPlaneNormalize(viewProjT.r[3] + viewProjT.r[0])); // Left
		XMStoreFloat4(&passData.Planes[1], XMPlaneNormalize(viewProjT.r[3] - viewProjT.r[0])); // Right
		XMStoreFloat4(&passData.Planes[2], XMPlaneNormalize(viewProjT.r[3] + viewProjT.r[1])); // Bottom
		XMStoreFloat4(&passData.Planes[3], XMPlaneNormalize(viewProjT.r[3] - viewProjT.r[1])); // Up
		XMStoreFloat4(&passData.Planes[4], XMPlaneNormalize(viewProjT.r[2]));				   // Near
		XMStoreFloat4(&passData.Planes[5], XMPlaneNormalize(viewProjT.r[3] - viewProjT.r[2])); // Far

		m_PassBuffer->LoadData(GetMainContext(), passData);
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

		uint32 groupsPerDispatch = MaxDispatchGroups * WAVE_THREADS_NUM;
		uint32 dispatchCount = DivRoundUp(m_InstanceCount, groupsPerDispatch);

		for (uint32 i = 0; i < dispatchCount; i++)
		{
			DispatchData dispatchData;
			dispatchData.DispatchInstanceOffset = groupsPerDispatch * i;
			dispatchData.DispatchInstanceCount = std::min(m_InstanceCount - dispatchData.DispatchInstanceOffset, groupsPerDispatch);

			m_DispatchDataBuffer->LoadData(GetMainContext(), dispatchData);

			m_Pso.CommitAll(GetMainContext(), m_Binder.get());

			uint32 groupCount = DivRoundUp(dispatchData.DispatchInstanceCount, WAVE_THREADS_NUM);
			static_cast<ID3D12GraphicsCommandList6*>(GetMainContext()->GetCommandCtx()->GetCmdList())->DispatchMesh(groupCount, 1, 1);
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_Always);
		ImGui::SetNextWindowSize({ 280, 150 }, ImGuiCond_Always);
		ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		
		if (ImGui::InputInt("Width", (int*)&m_GridSize.x) ||
			ImGui::InputInt("Height", (int*)&m_GridSize.y) ||
			ImGui::InputInt("Depth", (int*)&m_GridSize.z))
			BuildInstanceBuffer();

		const char* items[] = {
			"Meshlets",
			"Lods",
		};

		ImGui::Combo("Render Mode", (int*)&m_RenderMode, items, IM_ARRAYSIZE(items));

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void MeshShadersDemo::BuildInstanceBuffer()
	{
		uint32 w = m_GridSize.x;
		uint32 h = m_GridSize.y;
		uint32 d = m_GridSize.z;
		uint32 spacing = 0;
		float scale = 100.0f;

		m_InstanceCount = w * h * d;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(Instance) * m_InstanceCount;
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
		srvDesc.Buffer.NumElements = m_InstanceCount;
		srvDesc.Buffer.StructureByteStride = sizeof(Instance);
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;

		aiVector3D boundingCenter;
		float boundingRadius;
		m_Model[0]->GetBoundingSphere(boundingCenter, boundingRadius);

		XMMATRIX scaleMatrix = XMMatrixScaling(scale, scale, scale);
		boundingRadius *= scale;

		float diameter = boundingRadius * 2;
		float step = diameter + spacing;

		XMVECTOR extends = XMVectorSet(
			step * (w - 1) * 0.5f,
			step * (h - 1) * 0.5f,
			step * (d - 1) * 0.5f,
			0.0f
		);

		std::vector<Instance> instances(m_InstanceCount);

		for (uint32 x = 0; x < w; x++)
		{
			for (uint32 y = 0; y < h; y++)
			{
				for (uint32 z = 0; z < d; z++)
				{
					XMVECTOR instancePos = XMVectorSet(x * step, y * step, z * step, 1.0f);
					instancePos = XMVectorSubtract(instancePos, extends);

					XMMATRIX instanceWorld = XMMatrixMultiply(XMMatrixScaling(scale, scale, scale), XMMatrixTranslationFromVector(instancePos));

					XMVECTOR spherePos = XMVectorSet(boundingCenter.x, boundingCenter.y, boundingCenter.z, 1);
					spherePos = XMVector3TransformCoord(spherePos, instanceWorld);
					spherePos = XMVectorSetW(spherePos, boundingRadius);

					Instance instance;
					XMStoreFloat4x4(&instance.World, XMMatrixTranspose(instanceWorld));
					XMStoreFloat4(&instance.BoundingSphere, spherePos);

					uint32 index = x * h * d + y * d + z;
					instances[index] = instance;
				}
			}
		}

		m_InstanceBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), desc, QueueId::Direct);
		m_InstanceBuffer->LoadData(GetMainContext(), instances.data());
		m_InstanceBuffer->CreateSRV(&srvDesc);
		m_InstanceBuffer->SetName(L"m_InstanceBuffer");

		if (m_Binder)
		{
			m_Binder->DryMutableResources();
			BindResources();
		}
	}

	void MeshShadersDemo::BindResources()
	{
		for (size_t i = 0; i < m_LodCount; i++)
		{
			m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "gVertices", m_Model[i]->GetVertexBufferShared(), 0, i);
			m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "gMeshlets", m_Meshlet[i], 0, i);
			m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "gMeshletVertices", m_MeshletVertices[i], 0, i);
			m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "gMeshletIndices", m_MeshletTris[i], 0, i);
		}

		m_Binder->BindResource(EDU_SHADER_TYPE_AMPLIFICATION, "cbMeshletData", m_MeshletData);
		m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "cbMeshletData", m_MeshletData);

		m_Binder->BindResource(EDU_SHADER_TYPE_AMPLIFICATION, "gInstances", m_InstanceBuffer);
		m_Binder->BindResource(EDU_SHADER_TYPE_MESH, "gInstances", m_InstanceBuffer);

		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_AMPLIFICATION, "cbDispatchData", m_DispatchDataBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_MESH, "cbDispatchData", m_DispatchDataBuffer);

		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_AMPLIFICATION, "cbPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_MESH, "cbPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
	}

	bool MeshShadersDemo::ReadMeshlet(const char*			 filePath,
									  std::vector<Meshlet>&	 meshlets,
									  std::vector<CullData>& cull_data,
									  std::vector<uint32_t>& meshlet_vertices,
									  std::vector<uint32_t>& meshlet_triangles_packed)
	{
		struct FileHeader
		{
			uint32_t prolog;
			uint32_t version;
			uint32_t meshlets_count;
			uint32_t vertices_count;
			uint32_t triangles_count;
		};

		FileHeader fh = {};

		std::ifstream istream(filePath, std::ios::binary);

		istream.read(reinterpret_cast<char*>(&fh), sizeof(FileHeader));

		if (fh.prolog != 'LHSM')
		{
			ASSERT_FAILED("Expected file prolog: ", "MSHL");
			return false;
		}

		meshlets.resize(fh.meshlets_count);
		cull_data.resize(fh.meshlets_count);
		meshlet_vertices.resize(fh.vertices_count);
		meshlet_triangles_packed.resize(fh.triangles_count);

		istream.read(reinterpret_cast<char*>(meshlets.data()), sizeof(Meshlet) * fh.meshlets_count);
		istream.read(reinterpret_cast<char*>(cull_data.data()), sizeof(CullData) * fh.meshlets_count);
		istream.read(reinterpret_cast<char*>(meshlet_vertices.data()), sizeof(uint32_t) * fh.vertices_count);
		istream.read(reinterpret_cast<char*>(meshlet_triangles_packed.data()), sizeof(uint32_t) * fh.triangles_count);

		istream.close();

		return true;
	}
}