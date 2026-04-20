#include "VolumetricCloudsDemo.h"

#include <DemoHelpers.h>

namespace EduEngine
{
	void VolumetricCloudsDemo::ChangeInitInfo(EngineInitInfo& info)
	{
		
	}

	void VolumetricCloudsDemo::OnStartUp()
	{
		m_CloudsRendering = std::make_unique<VolumetricRaymarchedClouds>(GetDevice(), GetMainContext(), GetSwapChain()->GetWidth(), GetSwapChain()->GetHeight());
	}

	void VolumetricCloudsDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera(), 10);

		m_CloudsRendering->Update(GetMainContext(), timer, GetCamera());
	}

	void VolumetricCloudsDemo::OnRender(const Timer& timer)
	{
		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		m_CloudsRendering->Render(GetMainContext(), *GetSwapChain());

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		RenderImGui();

		RenderEngine::PopulateDebugImguiCommand();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void VolumetricCloudsDemo::OnResize()
	{
		if (m_CloudsRendering)
			m_CloudsRendering->Resize(GetSwapChain()->GetWidth(), GetSwapChain()->GetHeight());
	}

	void VolumetricCloudsDemo::RenderImGui()
	{
		auto constantsData = m_CloudsRendering->GetConstantsData();
		if (ImGui::Begin("Cloud Settings"))
		{
			ImGui::SliderFloat3("Sun Position", &constantsData.SunPosition.x, -1, 1);
			ImGui::SliderInt("Max Steps", (int*)&constantsData.MaxSteps, 10, 500);
			ImGui::SliderFloat("March Size", &constantsData.MarchSize, 0.01f, 1.0f);
			m_CloudsRendering->UpdateConstantsData(GetMainContext(), constantsData);

			ImGui::End();
		}
	}
}