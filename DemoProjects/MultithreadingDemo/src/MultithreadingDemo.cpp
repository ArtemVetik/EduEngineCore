#include "MultithreadingDemo.h"

#include <InputManager.h>

namespace EduEngine
{
	using namespace EduEngine::EduBinding;
	using namespace DirectX::SimpleMath;

	MultithreadingDemo::~MultithreadingDemo()
	{
		m_ExitApp = true;

		ReleaseSemaphore(m_WorkerSemaphore, m_ActiveThreads, nullptr);
		ReleaseSemaphore(m_MainSemaphore, m_ActiveThreads, nullptr);

		for (uint32 i = 0; i < m_Threads.size(); i++)
			m_Threads[i].join();

		CloseHandle(m_MainSemaphore);
		CloseHandle(m_WorkerSemaphore);
	}

	void MultithreadingDemo::ChangeInitInfo(EngineInitInfo& info)
	{
		info.ImmediateContextsNum = 0;
		info.DeferredContextsNum = std::thread::hardware_concurrency() - 1;
		
		m_Contexts.resize(1 + info.ImmediateContextsNum + info.DeferredContextsNum);
	}

	void MultithreadingDemo::OnStartUp()
	{
		struct Vertex
		{
			DirectX::XMFLOAT3 Pos;
			DirectX::XMFLOAT2 TexC;
		};

		Vertex vertices[] =
		{
			// +X
			{ { +1, +1, -1 }, { 0, 0 } }, { { +1, -1, -1 }, { 0, 1 } }, { { +1, -1, +1 }, { 1, 1 } }, { { +1, +1, +1 }, { 1, 0 } },
			// -X
			{ { -1, +1, +1 }, { 0, 0 } }, { { -1, -1, +1 }, { 0, 1 } }, { { -1, -1, -1 }, { 1, 1 } }, { { -1, +1, -1 }, { 1, 0 } },
			// +Y
			{ { -1, +1, +1 }, { 0, 0 } }, { { -1, +1, -1 }, { 0, 1 } }, { { +1, +1, -1 }, { 1, 1 } }, { { +1, +1, +1 }, { 1, 0 } },
			// -Y
			{ { -1, -1, -1 }, { 0, 0 } }, { { -1, -1, +1 }, { 0, 1 } }, { { +1, -1, +1 }, { 1, 1 } }, { { +1, -1, -1 }, { 1, 0 } },
			// +Z
			{ { -1, +1, +1 }, { 0, 0 } }, { { +1, +1, +1 }, { 0, 1 } }, { { +1, -1, +1 }, { 1, 1 } }, { { -1, -1, +1 }, { 1, 0 } },
			// -Z
			{ { +1, +1, -1 }, { 0, 0 } }, { { -1, +1, -1 }, { 0, 1 } }, { { -1, -1, -1 }, { 1, 1 } }, { { +1, -1, -1 }, { 1, 0 } },
		};

		uint16 indices[] = {
			0,2,1,		0,3,2,		// +X
			4,6,5,		4,7,6,      // -X
			8,10,9,		8,11,10,	// +Y
			12,14,13,	12,15,14,	// -Y
			16,18,17,	16,19,18,	// +Z
			20,22,21,	20,23,22,	// -Z
		};

		m_CubeVB = std::make_shared<VertexBufferD3D12>(GetDevice(), GetMainContext(), vertices, sizeof(Vertex), _countof(vertices));
		m_CubeIB = std::make_shared<IndexBufferD3D12>(GetDevice(), GetMainContext(), indices, sizeof(uint16), _countof(indices), DXGI_FORMAT_R16_UINT);

		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbPerObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = {};
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;

		auto vertexShader = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Color.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto pixelShader = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Color.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);

		D3D12_INPUT_ELEMENT_DESC inputLayout[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

		m_PSO.SetDepthStencilState(dss);
		m_PSO.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_PSO.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PSO.SetShader(vertexShader);
		m_PSO.SetShader(pixelShader);
		m_PSO.Build(GetDevice());

		m_ObjBuff = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueId::Direct);
		m_PassBuff = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueId::Direct);

		XMFLOAT3 pos = { 80, 80, -200 };
		XMFLOAT3 look = { 0, 0, 1 };
		XMFLOAT3 right = { 1, 0, 0 };
		GetCamera()->Setup(pos, look, right, { 0, 1, 0 });

		const wchar_t* texturePaths[]
		{
			L"assets/Textures/Yellow1.dds",
			L"assets/Textures/Blue1.dds",
			L"assets/Textures/Orange1.dds",
			L"assets/Textures/Green1.dds",
			L"assets/Textures/Red1.dds",
		};

		for (uint32 i = 0; i < TextureCount; i++)
		{
			Texture texture;
			texture.Load(texturePaths[i % TextureCount], GetDevice(), GetMainContext());

			m_Binder[i] = m_PSO.CreateShaderBinder();
			m_Binder[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuff);
			m_Binder[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuff);
			m_Binder[i]->BindResource(EDU_SHADER_TYPE_PIXEL, "gAlbedo", texture.GetD3D12Texture());
		}

		m_GridSize = { 5, 5, 5 };
		m_ActiveThreads = GetInitInfo().DeferredContextsNum / 2;

		m_MainSemaphore = CreateSemaphore(nullptr, 0, m_ActiveThreads, nullptr);
		m_WorkerSemaphore = CreateSemaphore(nullptr, 0, m_ActiveThreads, nullptr);

		m_Threads.resize(m_ActiveThreads);
		for (uint32 i = 0; i < m_ActiveThreads; i++)
		{
			m_Threads[i] = std::thread(ThreadWorker, this, i);
		}
	}

	void MultithreadingDemo::OnUpdate(const Timer& timer)
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

	void MultithreadingDemo::OnRender(const Timer& timer)
	{
		m_Timer = &timer;

		ReleaseSemaphore(m_WorkerSemaphore, m_ActiveThreads, nullptr);

		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		for (size_t i = 0; i < m_ActiveThreads; i++)
			WaitForSingleObject(m_MainSemaphore, INFINITE);

		m_Contexts[0] = GetMainContext()->GetCommandCtx();

		for (uint32 i = 0; i < m_ActiveThreads; i++)
			m_Contexts[i + 1] = GetDeferredContext(i)->GetCommandCtx();

		auto& dCommandQueue = GetDevice()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		dCommandQueue.CloseAndExecuteCommandContexts(m_Contexts.data(), m_ActiveThreads + 1);

		GetMainContext()->FinishFrame();

		for (uint32 i = 0; i < m_ActiveThreads; i++)
			GetDeferredContext(i)->FinishFrame();

		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_AlwaysAutoResize;

		ImGui::SetNextWindowPos(ImVec2(5.0f, 5.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));

		ImGui::Begin("Multithreading Demo", nullptr, flags);

		ImGui::SliderInt3("Grid Size", (int*)&m_GridSize, 1, 15);

		uint32 prevActiveThreads = m_ActiveThreads;
		if (ImGui::SliderInt("Thread Count", (int*)&m_ActiveThreads, 1, GetInitInfo().DeferredContextsNum))
		{
			m_ExitApp = true;

			ReleaseSemaphore(m_WorkerSemaphore, prevActiveThreads, nullptr);
			ReleaseSemaphore(m_MainSemaphore, prevActiveThreads, nullptr);

			for (uint32 i = 0; i < m_Threads.size(); i++)
				m_Threads[i].join();

			CloseHandle(m_MainSemaphore);
			CloseHandle(m_WorkerSemaphore);

			m_ExitApp = false;

			m_MainSemaphore = CreateSemaphore(nullptr, 0, m_ActiveThreads, nullptr);
			m_WorkerSemaphore = CreateSemaphore(nullptr, 0, m_ActiveThreads, nullptr);

			m_Threads.resize(m_ActiveThreads);
			for (uint32 i = 0; i < m_ActiveThreads; i++)
			{
				m_Threads[i] = std::thread(ThreadWorker, this, i);
			}
		}

		ImGui::Text("CPU Time: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::End();

		RenderEngine::PopulateDebugImguiCommand();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void MultithreadingDemo::ThreadWorker(MultithreadingDemo* pThis, uint64 contextId)
	{
		while (true)
		{
			WaitForSingleObject(pThis->m_WorkerSemaphore, INFINITE);

			if (pThis->m_ExitApp)
				break;

			pThis->GetDeferredContext(contextId)->BeginDeferredFrame(QueueId::Direct);

			uint32 numToRender = (pThis->m_GridSize.x * pThis->m_GridSize.y * pThis->m_GridSize.z) / pThis->m_ActiveThreads;
			uint32 startIndex = contextId * numToRender;

			if (contextId == pThis->m_ActiveThreads - 1)
				numToRender = (pThis->m_GridSize.x * pThis->m_GridSize.y * pThis->m_GridSize.z) - startIndex;

			for (uint32 idx = startIndex; idx < startIndex + numToRender; idx++)
			{
				uint32 x = idx % pThis->m_GridSize.x;
				uint32 y = (idx / pThis->m_GridSize.x) % pThis->m_GridSize.y;
				uint32 z = idx / (pThis->m_GridSize.x * pThis->m_GridSize.y);

				float radius = 40.0f;

				char sign = contextId % 2 == 0 ? 1 : -1;

				Matrix world = Matrix::CreateScale(10.0f) * Matrix::CreateRotationY(sign * pThis->m_Timer->GetTotalTime()) * Matrix::CreateTranslation(x * radius, y * radius, z * radius);

				XMFLOAT4X4 viewProj;
				XMStoreFloat4x4(&viewProj, XMMatrixTranspose(pThis->GetCamera()->GetViewProjMatrix()));

				pThis->m_ObjBuff->LoadData(pThis->GetDeferredContext(contextId), world.Transpose());
				pThis->m_PassBuff->LoadData(pThis->GetDeferredContext(contextId), viewProj);

				ID3D12DescriptorHeap* descriptorHeaps[] = { pThis->GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

				pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

				pThis->GetDeferredContext(contextId)->GetCommandCtx()->SetRenderTargets(1, &pThis->GetSwapChain()->CurrentBackBufferView(), true, &pThis->GetSwapChain()->DepthStencilView());
				pThis->GetDeferredContext(contextId)->GetCommandCtx()->SetViewports(&pThis->GetViewport(), 1);
				pThis->GetDeferredContext(contextId)->GetCommandCtx()->SetScissorRects(&pThis->GetScissorRect(), 1);

				pThis->m_PSO.CommitAll(pThis->GetDeferredContext(contextId), pThis->m_Binder[contextId % TextureCount].get());

				pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &pThis->m_CubeVB->GetView());
				pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&pThis->m_CubeIB->GetView());

				pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(pThis->m_CubeIB->GetLength(), 1, 0, 0, 0);
			}

			ReleaseSemaphore(pThis->m_MainSemaphore, 1, nullptr);
		}
	}
}