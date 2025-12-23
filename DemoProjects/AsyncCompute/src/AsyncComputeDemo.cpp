#include "AsyncComputeDemo.h"

#include <InputManager.h>

namespace EduEngine
{
	void AsyncComputeDemo::ChangeInitInfo(EngineInitInfo& info)
	{
		m_ComputeImmediateContextId = QueueId::Compute;

		info.CommandQueues = QueueId::Direct | QueueId::Compute;
		info.ImmediateContextsNum = 1;
		info.ImmediateContextsQueues = &m_ComputeImmediateContextId;
		
		info.QueryHeapSettings.NumQueries = 32;
		info.QueryHeapSettings.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	}

	void AsyncComputeDemo::OnStartUp()
	{
		GetDevice()->GetD3D12Device()->SetStablePowerState(TRUE);

		m_MaxParticles = 1000000;
		m_PingPongCounter = 0;
		m_EnableAsyncCompute = true;
		m_BuffersDirty = false;

		m_EmitPSO = std::make_unique<ComputePipelineState>(QueueId::Direct | QueueId::Compute);
		m_UpdatePSO = std::make_unique<ComputePipelineState>(QueueId::Direct | QueueId::Compute);
		m_DrawPSO = std::make_unique<PipelineState>();

		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc shaderDesc = {};
		shaderDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		shaderDesc.ResourceDesc = resDesc;
		shaderDesc.ResourceNum = _countof(resDesc);

		auto csEmit = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ParticlesCS.hlsl", L"CS_Emit", L"cs_6_0", nullptr, shaderDesc);
		auto csUpdate = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ParticlesCS.hlsl", L"CS_Update", L"cs_6_0", nullptr, shaderDesc);
		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ParticlesDraw.hlsl", L"VS", L"vs_6_0", nullptr, shaderDesc);
		auto gs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ParticlesDraw.hlsl", L"GS", L"gs_6_0", nullptr, shaderDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ParticlesDraw.hlsl", L"PS", L"ps_6_0", nullptr, shaderDesc);

		m_EmitPSO->SetShader(csEmit);
		m_EmitPSO->Build(GetDevice());

		m_UpdatePSO->SetShader(csUpdate);
		m_UpdatePSO->Build(GetDevice());

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].LogicOpEnable = false;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		m_DrawPSO->SetBlendState(blendDesc);
		m_DrawPSO->SetDepthStencilState(dss);
		m_DrawPSO->SetShader(vs);
		m_DrawPSO->SetShader(gs);
		m_DrawPSO->SetShader(ps);
		m_DrawPSO->SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
		m_DrawPSO->SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_DrawPSO->Build(GetDevice());

		m_ComputePassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_DrawPassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		BuildBuffers(false);
		BuildBinders();

		m_GpuStats = std::make_unique<GpuStats>(GetDevice());

		GetCamera()->Setup({ 0, 0, -10 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 });
	}

	void AsyncComputeDemo::OnUpdate(const Timer& timer)
	{
		const float WHEEL_SCALE = 2.0f;
		static float theta = 0;
		static float phi = 0;
		static float radius = 100.0f;
		static float rotationScale = 0.01f;

		auto mouseState = InputManager::GetInstance().GetMouseState();

		if ((mouseState.rgbButtons[1] & 0x80) != 0)
		{
			theta -= mouseState.lX * rotationScale;
			phi += mouseState.lY * rotationScale;
			phi = std::clamp(phi, -XM_PIDIV2, XM_PIDIV2);
		}

		float deltaZoom = (float)mouseState.lZ / WHEEL_DELTA * WHEEL_SCALE;
		radius = std::clamp(radius - deltaZoom, 2.0f, 200.0f);

		XMVECTOR pos = XMVectorSet
		(
			cos(phi) * cos(theta) * radius,
			sin(phi) * radius,
			cos(phi) * sin(theta) * radius,
			0.0f
		);

		XMVECTOR look = XMVector3Normalize(-pos);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		if (fabs(XMVectorGetY(look)) > 0.99f)
		{
			up = XMVectorSet(1, 0, 0, 0);
		}

		XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, look));
		XMVECTOR realUp = XMVector3Cross(look, right);

		XMFLOAT3 posF, lookF, rightF, upF;
		XMStoreFloat3(&posF, pos);
		XMStoreFloat3(&lookF, look);
		XMStoreFloat3(&rightF, right);
		XMStoreFloat3(&upF, realUp);

		GetCamera()->Setup(posF, lookF, rightF, upF);
		GetCamera()->Update(timer);

		m_GpuStats->Update(timer.GetDeltaTime());
	}

	void AsyncComputeDemo::OnRender(const Timer& timer)
	{
		DeviceContext* computeContext = m_EnableAsyncCompute ? GetImmediateContext(0) : GetMainContext();

		CommandQueueD3D12& directQueue = GetDevice()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		CommandQueueD3D12& computeQueue = GetDevice()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);

		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		if (m_EnableAsyncCompute)
			computeContext->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		ComputePassCB computePassCb = {};
		computePassCb.MaxParticlesNum = m_MaxParticles;
		computePassCb.DeltaTime = timer.GetDeltaTime();
		computePassCb.TotalTime = timer.GetTotalTime();
		computePassCb.EmitterSeed = rand() % UINT_MAX;

		m_ComputePassBuffer->LoadData(computeContext, computePassCb);

		if (m_BuffersDirty)
		{
			m_EmitPSO->CommitAll(computeContext, m_EmitBinder[m_PingPongCounter].get());
			computeContext->GetCommandCtx()->GetCmdList()->Dispatch(m_MaxParticles / NumThreads + 1, 1, 1);
			m_BuffersDirty = false;
		}

		m_GpuStats->MarkStartComputeWork(computeContext);
		m_UpdatePSO->CommitAll(computeContext, m_UpdateBinder[m_PingPongCounter].get());
		computeContext->GetCommandCtx()->GetCmdList()->Dispatch(m_MaxParticles / NumThreads + 1, 1, 1);

		if (m_EnableAsyncCompute)
			computeContext->GetCommandCtx()->InsertUAVBarrier(m_ParticlesBuffer[1 - m_PingPongCounter].get(), true);
		else
			computeContext->GetCommandCtx()->InsertUAVBarrier(m_ParticlesBuffer[m_PingPongCounter].get(), true);

		m_GpuStats->MarkEndComputeWork(computeContext);

		DrawPassCB drawPassCb = {};
		XMStoreFloat4x4(&drawPassCb.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		drawPassCb.AspectRatio = GetViewport().Width / GetViewport().Height;
		m_DrawPassBuffer->LoadData(GetMainContext(), drawPassCb);

		if (m_EnableAsyncCompute)
		{
			computeQueue.Wait(&directQueue, directQueue.GetNextCmdListNum() - 1);

			CommandContext* computeCtxs[]{ computeContext->GetCommandCtx() };
			computeQueue.CloseAndExecuteCommandContexts(computeCtxs, _countof(computeCtxs));
			computeContext->FinishFrame();
		}

		m_GpuStats->MarkStartDrawWork(GetMainContext());
		
		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		m_DrawPSO->CommitAll(GetMainContext(), m_DrawBinder[m_PingPongCounter].get());
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawInstanced(m_MaxParticles, 1, 0, 0);

		m_GpuStats->MarkEndDrawWork(GetMainContext());

		if (m_EnableAsyncCompute)
			m_PingPongCounter = 1 - m_PingPongCounter;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(320, 80), ImGuiCond_Always);

		ImGui::Begin("Settings", nullptr,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize);

		if (ImGui::Checkbox("Enable Async Compute", &m_EnableAsyncCompute))
		{
			BuildBuffers(true);
			BuildBinders();
		}

		if (ImGui::DragInt("Particles Num", (int*)&m_MaxParticles, 1, 10000000))
		{
			m_MaxParticles = std::clamp(m_MaxParticles, 1u, 10000000u);

			BuildBuffers(false);
			BuildBinders();
		}

		ImGui::End();

		m_GpuStats->DrawImGui(m_EnableAsyncCompute);
		RenderEngine::PopulateDebugImguiCommand();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void AsyncComputeDemo::BuildBuffers(bool onlySecond)
	{
		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(ParticleData) * m_MaxParticles;
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_MaxParticles;
		srvDesc.Buffer.StructureByteStride = sizeof(ParticleData);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = m_MaxParticles;
		uavDesc.Buffer.StructureByteStride = sizeof(ParticleData);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		QueueMask queueMask = QueueId::Direct;
		if (m_EnableAsyncCompute)
			queueMask |= QueueId::Compute;

		uint8 buffersNum = m_EnableAsyncCompute ? 2 : 1;

		for (uint8 i = onlySecond ? 1 : 0; i < buffersNum; i++)
		{
			m_ParticlesBuffer[i] = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, queueMask);
			m_ParticlesBuffer[i]->CreateSRV(&srvDesc);
			m_ParticlesBuffer[i]->CreateUAV(&uavDesc);
		}

		if (!m_EnableAsyncCompute)
		{
			m_ParticlesBuffer[1] = nullptr;
		}

		m_PingPongCounter = 0;

		if (!onlySecond)
			m_BuffersDirty = true;
	}

	void AsyncComputeDemo::BuildBinders()
	{
		uint8 buffersNum = m_EnableAsyncCompute ? 2 : 1;

		for (uint8 i = 0; i < buffersNum; i++)
		{
			m_EmitBinder[i] = m_EmitPSO->CreateShaderBinder();
			m_UpdateBinder[i] = m_UpdatePSO->CreateShaderBinder();
			m_DrawBinder[i] = m_DrawPSO->CreateShaderBinder();

			m_EmitBinder[i]->BindResource(EDU_SHADER_TYPE_COMPUTE, "gParticlesIn", m_ParticlesBuffer[i]);
			m_EmitBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_ComputePassBuffer);

			m_UpdateBinder[i]->BindResource(EDU_SHADER_TYPE_COMPUTE, "gParticlesIn", m_ParticlesBuffer[i]);
			m_UpdateBinder[i]->BindResource(EDU_SHADER_TYPE_COMPUTE, "gParticlesOut", m_ParticlesBuffer[buffersNum == 1 ? 0 : (1 - i)]);
			m_UpdateBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_ComputePassBuffer);

			m_DrawBinder[i]->BindResource(EDU_SHADER_TYPE_VERTEX, "gParticles", m_ParticlesBuffer[i]);
			m_DrawBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_DrawPassBuffer);
			m_DrawBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_GEOMETRY, "cbPass", m_DrawPassBuffer);
		}

		if (!m_EnableAsyncCompute)
		{
			m_EmitBinder[1] = nullptr;
			m_UpdateBinder[1] = nullptr;
			m_DrawBinder[1] = nullptr;
		}
	}
}