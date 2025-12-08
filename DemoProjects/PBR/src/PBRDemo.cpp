#include "PBRDemo.h"

#include "../../InputSystem/include/InputManager.h"
#include "../../ShaderBinding/EduBinding/include/PipelineState.h"

#include <FileUtils.h>

namespace EduEngine
{
	void PBRDemo::OnStartUp()
	{
		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), "assets\\Models\\DamagedHelmet.gltf");
		m_Mesh->Load();

		m_MeshScale = 50.0f;
		m_MeshRotation = { -90.0, 90.0, 0.0 };

		XMFLOAT3 camPos = { -150, 0, 0 };
		XMFLOAT3 camDir = { 1, 0, 0 };
		XMFLOAT3 camUp = { 0, 1, 0 };
		XMFLOAT3 camRight = { 0, 0, -1 };
		GetCamera()->Setup(camPos, camDir, camRight, camUp);

		m_AlbedoTexture.Load(L"assets\\Textures\\DamagedHelmet\\Default_albedo.dds", GetDevice(), GetMainContext(), nullptr, L"Tex Albedo");
		m_MetallicRoughnessTexture.Load(L"assets\\Textures\\DamagedHelmet\\Default_metalRoughness.dds", GetDevice(), GetMainContext(), nullptr, L"Tex MetalRough");
		m_AOTexture.Load(L"assets\\Textures\\DamagedHelmet\\Default_AO.dds", GetDevice(), GetMainContext(), nullptr, L"Tex AO");
		m_NormalMapTexture.Load(L"assets\\Textures\\DamagedHelmet\\Default_normal.dds", GetDevice(), GetMainContext(), nullptr, L"Tex NormalMap");

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueMask::Direct);
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueMask::Direct);

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

		m_MaterialConstants = { };
		m_MaterialBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, &m_MaterialConstants, QueueMask::Direct);
		m_MaterialBuffer->SetName(L"Buffer Material");

		m_LightConstants = {};
		m_LightBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice(), QueueMask::Direct);
		m_LightBuffer->LoadData(GetMainContext(), m_LightConstants);
		m_LightBuffer->CreateSRV(GetMainContext(), 1, sizeof(PBRLighting::Light));

		m_Prepass = std::make_shared<PBRPrepass>(GetDevice(), GetMainContext());
		m_Prepass->GenerateTextures("assets\\Textures\\HDR\\shanghai_bund_4k.hdr", GetDevice(), GetMainContext());

		m_PBRTextured = true;
		
		BuildPBRPass();

		m_DebugRenderer = std::make_shared<DebugRendererSystem>(GetDevice());
	}

	void PBRDemo::OnUpdate(const Timer& timer)
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

		PBRLighting::ObjectConstants objConstants;
		objConstants.World = (SimpleMath::Matrix::CreateScale(m_MeshScale) *
			SimpleMath::Matrix::CreateRotationX(XMConvertToRadians(m_MeshRotation.x)) *
			SimpleMath::Matrix::CreateRotationY(XMConvertToRadians(m_MeshRotation.y)) *
			SimpleMath::Matrix::CreateRotationZ(XMConvertToRadians(m_MeshRotation.z))).Transpose();

		PBRLighting::PassConstants passConstants;
		XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passConstants.CamPos = GetCamera()->GetPosition();
		passConstants.DirectionalLightsCount = 1;
		passConstants.PrefilteredMapLods = PBRPrepass::PREFILTERED_MIP_LEVELS;

		m_ObjBuffer->LoadData(GetMainContext(), objConstants);
		m_PassBuffer->LoadData(GetMainContext(), passConstants);

		m_LightBuffer->LoadData(GetMainContext(), m_LightConstants);
		m_LightBuffer->CreateSRV(GetMainContext(), 1, sizeof(PBRLighting::Light));
	}

	void PBRDemo::OnRender(const Timer& timer)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { GetDevice()->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		GetMainContext()->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(GetSwapChain()->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		GetMainContext()->GetCommandCtx()->FlushResourceBarriers();
		GetMainContext()->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		GetMainContext()->GetCommandCtx()->SetRenderTargets(1, &GetSwapChain()->CurrentBackBufferView(), true, &GetSwapChain()->DepthStencilView());
		GetMainContext()->GetCommandCtx()->SetViewports(&GetViewport(), 1);
		GetMainContext()->GetCommandCtx()->SetScissorRects(&GetScissorRect(), 1);

		const float clear[4] = { 0, 0, 0, 1 };
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(GetSwapChain()->CurrentBackBufferView(), clear, 0, nullptr);
		GetMainContext()->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(GetSwapChain()->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

		m_ColorPass->GetPipelineState().CommitAll(GetMainContext(), m_Binder.get());

		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &(m_Mesh->GetVertexBuffer()->GetView()));
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&(m_Mesh->GetIndexBuffer()->GetView()));
		GetMainContext()->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexBuffer()->GetLength(), 1, 0, 0, 0);

		m_Prepass->RenderSky(GetDevice(), GetMainContext(), GetCamera());

		XMFLOAT3 lightEndDir =
		{
			m_LightConstants.Position.x + m_LightConstants.Direction.x * 20,
			m_LightConstants.Position.y + m_LightConstants.Direction.y * 20,
			m_LightConstants.Position.z + m_LightConstants.Direction.z * 20,
		};
		m_DebugRenderer->DrawSphere(5.0f, { 1, 1, 0 }, DirectX::XMMatrixTranslation(m_LightConstants.Position.x, m_LightConstants.Position.y, m_LightConstants.Position.z), 16);
		m_DebugRenderer->DrawArrow(m_LightConstants.Position, lightEndDir, { 1, 1, 0 }, { 1, 0, 0 });

		m_DebugRenderer->Render(GetMainContext(), GetCamera()->GetViewProjMatrix(), GetCamera()->GetPosition());

		bool genEnvMap;
		char envMapPath[MAX_PATH];
		RenderImGui(genEnvMap, envMapPath);

		if (genEnvMap)
		{
			m_Prepass->GenerateTextures(envMapPath, GetDevice(), GetMainContext());
		}
	}

	void PBRDemo::RenderImGui(bool& genEnvMap, char* envMapFile)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		struct PBRTexLoad
		{
			const char* Name;
			const char* BindName;
			Texture* PBRTex = nullptr;
		};

		PBRTexLoad pbrTexLoad[]
		{
			{ "Albedo: ", "gAlbedo", &m_AlbedoTexture},
			{ "MetalRough: ", "gMetallicRoughness", &m_MetallicRoughnessTexture},
			{ "AO: ", "gAO", &m_AOTexture },
			{ "NormalMap: ", "gNormalMap", &m_NormalMapTexture },
		};

		genEnvMap = false;

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(350, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);

		ImGui::Begin("Editor", nullptr,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize);

		if (ImGui::Checkbox("PBR Textured", &m_PBRTextured))
		{
			BuildPBRPass();
		}

		if (ImGui::CollapsingHeader("Light Settings"))
		{

			ImGui::DragFloat3("Position", (float*)&m_LightConstants.Position);
			if (ImGui::SliderFloat3("Direction", (float*)&m_LightConstants.Direction, -1.0f, 1.0f))
			{
				XMVECTOR dir = XMLoadFloat3(&m_LightConstants.Direction);
				dir = XMVector3Normalize(dir);
				XMStoreFloat3(&m_LightConstants.Direction, dir);
			}

			ImGui::ColorEdit3("Color", (float*)&m_LightConstants.Color);
			ImGui::SliderFloat("Strength", &m_LightConstants.Strength, 0.0f, 10.0f);
		}

		if (ImGui::CollapsingHeader("Mesh Settings"))
		{
			if (ImGui::Button("Load Model"))
			{
				char selectedFile[MAX_PATH];
				memset(selectedFile, 0, sizeof(char) * MAX_PATH);
				if (FileUtils::OpenFile("Models\0*.fbx;*.obj;*.gltf;\0", selectedFile))
				{
					m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), selectedFile);
					m_Mesh->Load();
				}
			}

			ImGui::DragFloat("Scale", &m_MeshScale);
			ImGui::DragFloat3("Rotation", (float*)&m_MeshRotation, 1.0f, -180.0, 180.0);
		}

		if (m_PBRTextured)
		{
			if (ImGui::CollapsingHeader("PBR Textures"))
			{
				if (ImGui::ColorEdit4("Diffuse Albedo", (float*)&m_MaterialConstants.DiffuseAlbedo))
				{
					m_MaterialBuffer->LoadData(GetMainContext(), & m_MaterialConstants);
				}

				ImVec2 previewSize(128, 128);

				for (uint8 i = 0; i < _countof(pbrTexLoad); i++)
				{
					ImGui::Text(pbrTexLoad[i].Name);
					ImGui::SameLine();
					ImGui::Text("(%s)", pbrTexLoad[i].BindName);
					ImGui::SameLine();
					if (ImGui::Button(("Load##" + std::to_string(i)).c_str()))
					{
						wchar_t selectedFileW[MAX_PATH];
						memset(selectedFileW, 0, sizeof(wchar_t) * MAX_PATH);
						if (FileUtils::OpenFileW(L"Models\0*.dds\0", selectedFileW))
						{
							auto tex = pbrTexLoad[i].PBRTex;
							tex->Load(selectedFileW, GetDevice(), GetMainContext());
							m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, pbrTexLoad[i].BindName, tex->GetD3D12Texture());
						}
					}
					if (pbrTexLoad[i].PBRTex->GetGPUPtr())
						ImGui::Image((ImTextureID)(intptr_t)pbrTexLoad[i].PBRTex->GetGPUPtr(), previewSize);

					ImGui::Separator();
				}
			}
		}
		else
		{
			if (ImGui::CollapsingHeader("PBR Settings"))
			{

				if (ImGui::ColorEdit4("Diffuse Albedo", (float*)&m_MaterialConstants.DiffuseAlbedo) ||
					ImGui::SliderFloat("Roughness", &m_MaterialConstants.Roughness, 0.0f, 1.0f) ||
					ImGui::SliderFloat("Metallic", &m_MaterialConstants.Metallic, 0.0f, 1.0f) ||
					ImGui::SliderFloat("AO", &m_MaterialConstants.AO, 0.0f, 1.0f))
				{
					m_MaterialBuffer->LoadData(GetMainContext(), &m_MaterialConstants);
				}
			}
		}

		if (ImGui::CollapsingHeader("Environment"))
		{
			if (ImGui::Button("Load Environment"))
			{
				memset(envMapFile, 0, sizeof(char) * MAX_PATH);
				if (FileUtils::OpenFile("HDR files\0*.hdr\0", envMapFile))
					genEnvMap = true;
			}

			static float lod = 0.0f;
			static int current = 0;
			const char* items[] = { "EnvMap", "IrradianceMap", "PrefilteredMap" };

			if (ImGui::Combo("Type", &current, items, IM_ARRAYSIZE(items)))
			{
				if (current == 0) m_Prepass->SetSkyTex(m_Prepass->GetHDREnvCubeMap());
				else if (current == 1) m_Prepass->SetSkyTex(m_Prepass->GetIrradianceMap());
				else if (current == 2) m_Prepass->SetSkyTex(m_Prepass->GetPrefilteredMap());
			}

			if (ImGui::SliderFloat("Lod", &lod, 0.0f, PBRPrepass::PREFILTERED_MIP_LEVELS))
			{
				m_Prepass->SetSkyLod(lod);
			}
		}

		if (ImGui::CollapsingHeader("Debug View"))
		{
			static int currentView = 0;
			const char* debugViews[] = 
			{ 
				"NONE", "DEBUGVIEW_ROUGHNESS", "DEBUGVIEW_METALLIC", "DEBUGVIEW_AO",
				"DEBUGVIEW_NORMAL", "DEBUGVIEW_DIFFUSE_IBL", "DEBUGVIEW_SPECULAR_IBL",
				"DEBUGVIEW_NDOTV", "DEBUGVIEW_FRESNEL", "DEBUGVIEW_BRDF_Y", "DEBUGVIEW_BRDF_X",
			};
			if (ImGui::Combo("Type##DebugView", &currentView, debugViews, IM_ARRAYSIZE(debugViews)))
			{
				UINT cSize = strlen(debugViews[currentView]) + 1;
				wchar_t* wc = new wchar_t[cSize];
				mbstowcs(wc, debugViews[currentView], cSize);

				BuildPBRPass(wc);
				
				delete[] wc;
			}
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void PBRDemo::BuildPBRPass(const wchar_t* debugView)
	{
		LPCWSTR macros[]
		{
			L"PBR_TEXTURED", (m_PBRTextured ? L"1" : L"0"),
			debugView, L"1",
			NULL, NULL,
		};

		m_ColorPass = std::make_unique<PBRLighting>(GetDevice(), macros);
		m_ColorPass->Build(GetDevice());
		m_ColorPass->GetPipelineState().SetName(L"PSO_PBR");

		m_Binder = m_ColorPass->GetPipelineState().CreateShaderBinder();

		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gAlbedo", m_AlbedoTexture.GetD3D12Texture());
		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gMetallicRoughness", m_MetallicRoughnessTexture.GetD3D12Texture());
		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gAO", m_AOTexture.GetD3D12Texture());
		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gNormalMap", m_NormalMapTexture.GetD3D12Texture());
		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gIrradianceMap", m_Prepass->GetIrradianceMap());
		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gPrefilteredMap", m_Prepass->GetPrefilteredMap());
		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gBRDFLut", m_Prepass->GetBrdfLut());

		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbMaterial", m_MaterialBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gLight", m_LightBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_VERTEX, "cbPerPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbPerPass", m_PassBuffer);
	}
}