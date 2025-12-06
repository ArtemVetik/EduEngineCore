#include "MultithreadingDemo.h"
#include <InputManager.h>

namespace EduEngine
{
	using namespace EduEngine::EduBinding;
	using namespace DirectX::SimpleMath;

	void MultithreadingDemo::ChangeInitInfo(EngineInitInfo& info)
	{
		info.NumDeferredContexts = 6;
		m_Contexts.resize(1 + info.NumDeferredContexts);
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

		ShaderDesc sDesc = {};
		sDesc.ResourceNum = 0;
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;

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

		m_ObjBuff = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueID::Direct);
		m_PassBuff = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueID::Direct);

		m_PSO.GetShaderBinder()->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuff);
		m_PSO.GetShaderBinder()->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuff);

		m_Threads.resize(GetInitInfo().NumDeferredContexts);
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
		for (uint32 i = 0; i < GetInitInfo().NumDeferredContexts; i++)
		{
			m_Threads[i] = std::thread(ThreadWorker, this, timer, i);
		}

		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		for (std::thread& thread : m_Threads)
		{
			thread.join();
		}

		m_Contexts[0] = GetMainContext()->GetCommandCtx();

		for (uint32 i = 0; i < GetInitInfo().NumDeferredContexts; i++)
			m_Contexts[i + 1] = GetDeferredContext(i)->GetCommandCtx();

		auto& dCommandQueue = GetDevice()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		dCommandQueue.CloseAndExecuteCommandContexts(m_Contexts.data(), m_Contexts.size());

		GetMainContext()->FinishFrame();
		GetMainContext()->GetCommandCtx()->Reset();

		for (uint32 i = 0; i < GetInitInfo().NumDeferredContexts; i++)
		{
			GetDeferredContext(i)->FinishFrame();
			GetDeferredContext(i)->GetCommandCtx()->Reset();
		}
	}

	void MultithreadingDemo::ThreadWorker(MultithreadingDemo* pThis, const Timer& timer, uint64 contextId)
	{
		float radius = 40.0f;
		float x = cos(XM_2PI * contextId / (float)pThis->GetInitInfo().NumDeferredContexts) * radius;
		float y = sin(XM_2PI * contextId / (float)pThis->GetInitInfo().NumDeferredContexts) * radius;

		char sign = contextId % 2 == 0 ? 1 : -1;

		Matrix world = Matrix::CreateScale(10.0f) * Matrix::CreateRotationY(sign * timer.GetTotalTime()) * Matrix::CreateTranslation(x, y, 0);
		
		XMFLOAT4X4 viewProj;
		XMStoreFloat4x4(&viewProj, XMMatrixTranspose(pThis->GetCamera()->GetViewProjMatrix()));

		pThis->m_ObjBuff->LoadData(pThis->GetDeferredContext(contextId), world.Transpose());
		pThis->m_PassBuff->LoadData(pThis->GetDeferredContext(contextId), viewProj);

		ID3D12DescriptorHeap* descriptorHeaps[] = { pThis->GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		pThis->GetDeferredContext(contextId)->GetCommandCtx()->SetRenderTargets(1, &pThis->GetSwapChain()->CurrentBackBufferView(), true, &pThis->GetSwapChain()->DepthStencilView());
		pThis->GetDeferredContext(contextId)->GetCommandCtx()->SetViewports(&pThis->GetViewport(), 1);
		pThis->GetDeferredContext(contextId)->GetCommandCtx()->SetScissorRects(&pThis->GetScissorRect(), 1);

		pThis->m_PSO.CommitAll(pThis->GetDeferredContext(contextId));

		pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &pThis->m_CubeVB->GetView());
		pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&pThis->m_CubeIB->GetView());

		pThis->GetDeferredContext(contextId)->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(pThis->m_CubeIB->GetLength(), 1, 0, 0, 0);
	}
}