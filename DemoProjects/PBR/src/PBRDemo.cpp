#include "PBRDemo.h"

#include "../../InputSystem/include/InputManager.h"
#include "../../ShaderBinding/EduBinding/include/PipelineState.h"

namespace EduEngine
{
	void PBRDemo::OnStartUp()
	{
		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), "Models\\DamagedHelmet.gltf");
		m_Mesh->Load();

		m_AlbedoTexture = std::make_shared<Texture>(GetDevice(), L"Textures\\Default_albedo.dds");
		m_MetallicRoughnessTexture = std::make_shared<Texture>(GetDevice(), L"Textures\\Default_metalRoughness.dds");
		m_AOTexture = std::make_shared<Texture>(GetDevice(), L"Textures\\Default_AO.dds");
		m_NormalMapTexture = std::make_shared<Texture>(GetDevice(), L"Textures\\Default_normal.dds");

		m_AlbedoTexture->Load(GetMainContext());
		m_MetallicRoughnessTexture->Load(GetMainContext());
		m_AOTexture->Load(GetMainContext());
		m_NormalMapTexture->Load(GetMainContext());

		m_ColorPass = std::make_unique<PBRLighting>(GetDevice());
		m_ColorPass->Build(GetDevice());

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueID::Direct);
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueID::Direct);

		PBRLighting::MaterialConstants materialConstants = {};

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.Width = (sizeof(PBRLighting::MaterialConstants) + 255) & ~255;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		std::shared_ptr<BufferD3D12> matBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, &materialConstants, QueueID::Direct);

		m_LightConstants = {};
		m_LightBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueID::Direct);
		m_LightBuffer->LoadData(m_LightConstants);
		m_LightBuffer->CreateSRV(1, sizeof(PBRLighting::Light));

		auto binder = m_ColorPass->GetPipelineState().GetShaderBinder();

		binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gAlbedo", m_AlbedoTexture->GetD3D12Texture());
		binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gMetallicRoughness", m_MetallicRoughnessTexture->GetD3D12Texture());
		binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gAO", m_AOTexture->GetD3D12Texture());
		binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gNormalMap", m_NormalMapTexture->GetD3D12Texture());

		binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbMaterial", matBuffer);
		binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gLight", m_LightBuffer);
		binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_VERTEX, "cbPerPass", m_PassBuffer);
		binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbPerPass", m_PassBuffer);

		m_DebugRenderer = std::make_shared<DebugRendererSystem>(GetDevice());
	}

	void PBRDemo::OnUpdate(const Timer& timer)
	{
		XMVECTOR direction = XMLoadFloat3(&GetCamera()->GetLook());
		XMVECTOR lrVector = XMLoadFloat3(&GetCamera()->GetRight());
		XMVECTOR upVector = XMLoadFloat3(&GetCamera()->GetUp());

		float moveScale = 35.0f;
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

#ifdef EDUBINDINGDEBUG
		if (InputManager::GetInstance().IsKeyDown(DIK_P))
			m_ColorPass->GetPipelineState().DebugPrint();
#endif

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
	
	void PBRDemo::OnRender(const Timer& timer)
	{
		PBRLighting::ObjectConstants objConstants;
		objConstants.World = (SimpleMath::Matrix::CreateScale(50.0f) * SimpleMath::Matrix::CreateRotationX(XM_PIDIV2) * SimpleMath::Matrix::CreateRotationY(XM_PI)).Transpose();

		PBRLighting::PassConstants passConstants;
		XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passConstants.CamPos = GetCamera()->GetPosition();

		m_LightConstants.Direction.x = cos(timer.GetTotalTime() * 0.5f);
		m_LightConstants.Direction.z = sin(timer.GetTotalTime() * 0.5f);
		m_LightConstants.Direction.y = -1.0f;
		m_LightConstants.Strength = { 1, 1, 1 };
		m_LightConstants.Position = XMFLOAT3(-m_LightConstants.Direction.x * 20, 80, -m_LightConstants.Direction.z * 20);

		m_ObjBuffer->LoadData(objConstants);
		m_PassBuffer->LoadData(passConstants);
		
		m_LightBuffer->LoadData(m_LightConstants);
		m_LightBuffer->CreateSRV(1, sizeof(PBRLighting::Light));

		m_ColorPass->GetPipelineState().CommitAll(GetMainContext());

		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &(m_Mesh->GetVertexBuffer()->GetView()));
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&(m_Mesh->GetIndexBuffer()->GetView()));
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexBuffer()->GetLength(), 1, 0, 0, 0);

		m_DebugRenderer->DrawSphere(10.0f, { 1, 1, 0 }, DirectX::XMMatrixTranslation(m_LightConstants.Position.x, m_LightConstants.Position.y, m_LightConstants.Position.z), 16);

		m_DebugRenderer->Render(GetMainContext(), GetCamera()->GetViewProjMatrix(), GetCamera()->GetPosition());

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}
}