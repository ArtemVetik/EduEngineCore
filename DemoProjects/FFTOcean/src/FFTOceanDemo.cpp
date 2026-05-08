#include "FFTOceanDemo.h"

#include <DemoHelpers.h>
#include <d3dx12.h>

namespace EduEngine
{
	void FFTOceanDemo::OnStartUp()
	{
		ShaderResourceDesc resDesc[] = 
		{
			{ "cbPass", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto hs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"Hull", L"hs_6_6", nullptr, sDesc);
		auto ds = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"Domain", L"ds_6_6", nullptr, sDesc);
		auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FFTOceanDraw.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = TRUE;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

		D3D12_INPUT_ELEMENT_DESC elementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_LAYOUT_DESC inputLayout = {};
		inputLayout.NumElements = _countof(elementDesc);
		inputLayout.pInputElementDescs = elementDesc;

		CD3DX12_RASTERIZER_DESC rast(D3D12_DEFAULT);
		rast.FillMode = D3D12_FILL_MODE_WIREFRAME;

		m_DrawPSO.SetShader(vs);
		m_DrawPSO.SetShader(ps);
		m_DrawPSO.SetShader(hs);
		m_DrawPSO.SetShader(ds);
		m_DrawPSO.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
		m_DrawPSO.SetRasterizerState(rast);
		m_DrawPSO.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_DrawPSO.SetDepthStencilState(dss);
		m_DrawPSO.SetInputLayout(inputLayout);
		m_DrawPSO.Build(GetDevice());

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.Width = sizeof(ConstantsData);
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		m_ConstantsBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, QueueId::Direct);
		m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);

		m_DrawBinder = m_DrawPSO.CreateShaderBinder();
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_HULL, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_DOMAIN, "cbPass", m_PassBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_HULL, "cbConstants", m_ConstantsBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_DOMAIN, "cbConstants", m_ConstantsBuffer);

		m_MeshGenerator = std::make_unique<MeshGenerator>(GetDevice(), GetMainContext(), 128, 128, 1024);

		GetCamera()->Setup({ 0, 50, -150 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 });
	}

	void FFTOceanDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera(), 100.0f);

		struct PassData
		{
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CameraPos;
		} passData;

		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passData.CameraPos = GetCamera()->GetPosition();

		m_PassBuffer->LoadData(GetMainContext(), passData);
	}

	void FFTOceanDemo::OnRender(const Timer& timer)
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

		m_DrawPSO.BeginPSOAndCommitResources(GetMainContext(), m_DrawBinder.get());
		
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_MeshGenerator->GetVertexBufferView());
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_MeshGenerator->GetIndexBufferView());

		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_MeshGenerator->GetIndexCount(), 1, 0, 0, 0);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Settings", nullptr);

		if (ImGui::DragInt("Max LOD Level", (int*)&m_ConstantsData.MaxLODLevel, 1, 0, 16))
			m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
		
		if (ImGui::DragInt("Tesselation Level", (int*)&m_ConstantsData.TesselationLevel, 1, 1, 100))
			m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
		
		if (ImGui::DragFloat("Max Tesselation Distance", &m_ConstantsData.MaxTesselationDistance, 1, 1, 10000))
			m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
		
		if (ImGui::DragFloat("Tesselation Decay Factor", &m_ConstantsData.TesselationDecayFactor, 0.1f, 1, 10))
			m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);
		
		if (ImGui::DragFloat("Culling Tollerance", &m_ConstantsData.CullingTollerance, 0.1f, 0, 10))
			m_ConstantsBuffer->LoadData(GetMainContext(), &m_ConstantsData);

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}
}