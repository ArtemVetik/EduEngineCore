#include "PBRDemo.h"

#include "../../InputSystem/include/InputManager.h"
#include "../../ShaderBinding/EduBinding/include/PipelineState.h"

#include <FileUtils.h>
#include <DemoHelpers.h>

namespace EduEngine
{
	const char* Models[]
	{
		"assets\\Models\\DamagedHelmet.gltf",
		"assets\\Models\\BarramundiFish.gltf",
		"assets\\Models\\BoomBox.gltf",
		"assets\\Models\\TransmissionTest.gltf",
	};

	const char* Textures[]
	{
		"assets\\Textures\\DamagedHelmet\\",
		"assets\\Textures\\BarramundiFish\\",
		"assets\\Textures\\BoomBox\\",
		"assets\\Textures\\TransmissionTest\\",
	};

	void PBRDemo::OnStartUp()
	{
		MeshLoadDesc meshDesc = {};
		meshDesc.Flags = MESH_LOAD_FLAG_LOAD_TEXTURES;
		meshDesc.TextureLoadDesc.Flags = TextureLoadDesc::CREATE_SRV;
		meshDesc.TextureLoadDesc.OnCPU = false;
		meshDesc.TextureBasePath = Textures[0];
		meshDesc.TextureExt = ".dds";

		m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), Models[0]);
		m_Mesh->Load(meshDesc);

		m_MeshScale = 50.0f;
		m_MeshRotation = { 0, 90.0, 0.0 };

		XMFLOAT3 camPos = { -150, 0, 0 };
		XMFLOAT3 camDir = { 1, 0, 0 };
		XMFLOAT3 camUp = { 0, 1, 0 };
		XMFLOAT3 camRight = { 0, 0, -1 };
		GetCamera()->Setup(camPos, camDir, camRight, camUp);

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());

		m_LightConstants = {};
		m_LightBuffer = std::make_shared<DynamicUploadBuffer>(GetDevice());
		m_LightBuffer->LoadData(GetMainContext(), m_LightConstants);
		m_LightBuffer->CreateSRV(GetMainContext(), 1, sizeof(PBRLighting::Light));

		m_Prepass = std::make_shared<IBLRendering>(GetDevice(), GetMainContext());
		m_Skybox = std::make_shared<Skybox>("assets\\Textures\\HDR\\shanghai_bund_4k.hdr", GetDevice(), GetMainContext(), m_Prepass.get());

		m_TextureIndexes = {};
		m_TextureIndexes.IrradianceMapIdx = m_Skybox->GetIrradianceMap()->GetSRVView()->GetGpuHeapIndex();
		m_TextureIndexes.PrefilteredMapIdx = m_Skybox->GetPrefilteredMap()->GetSRVView()->GetGpuHeapIndex();
		m_TextureIndexes.BRDFLutIdx = m_Prepass->GetBrdfLut()->GetSRVView()->GetGpuHeapIndex();
		m_TextureIndexes.MaterialBufferIdx = m_Mesh->GetMaterials()->GetSRVView()->GetGpuHeapIndex();

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.Width = (sizeof(PBRLighting::TextureIndexes) + 255) & ~255; // TODO: create align function
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		m_TextureIdxBuffer = std::make_shared<BufferD3D12>(GetDevice(), GetMainContext(), buffDesc, &m_TextureIndexes, QueueId::Direct);
		m_TextureIdxBuffer->SetName(L"TextureIndexes");

		BuildPBRPass();

		m_DebugRenderer = std::make_shared<DebugRendererSystem>(GetDevice());
	}

	void PBRDemo::OnUpdate(const Timer& timer)
	{
		FreeCameraUpdate(timer, GetCamera());

#ifdef EDUBINDINGDEBUG
		if (InputManager::GetInstance().IsKeyDown(DIK_P))
			m_ColorPass->GetPipelineState().DebugPrint();
#endif

		PBRLighting::PassConstants passConstants;
		XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(GetCamera()->GetViewProjMatrix()));
		passConstants.CamPos = GetCamera()->GetPosition();
		passConstants.DirectionalLightsCount = 1;
		passConstants.PrefilteredMapLods = IBLRendering::PREFILTERED_MIP_LEVELS;

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

		auto GetTexIdx = [&](Texture* texture) -> UINT
			{
				if (texture)
					return texture->GetD3D12Texture()->GetSRVView()->GetGpuHeapIndex();

				return -1;
			};

		m_ColorPass->GetOpaquePipelineState().BeginPSO(GetMainContext());
		for (uint32 i = 0; i < m_Mesh->GetOpaqueMeshCount(); i++)
		{
			uint32 meshIdx = m_Mesh->GetOpaqueMeshIndex(i);

			PBRLighting::ObjectConstants objData;

			objData.World = (SimpleMath::Matrix::CreateScale(m_MeshScale) *
				SimpleMath::Matrix::CreateRotationX(XMConvertToRadians(m_MeshRotation.x)) *
				SimpleMath::Matrix::CreateRotationY(XMConvertToRadians(m_MeshRotation.y)) *
				SimpleMath::Matrix::CreateRotationZ(XMConvertToRadians(m_MeshRotation.z))).Transpose();

			objData.AlbedoTexIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_BASE_COLOR));
			objData.NormalMapIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_NORMAL_MAP));
			objData.ORMTexIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_ORM));
			objData.TransmissionTexIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_TRANSMISSION));
			objData.ObjMaterialIdx = m_Mesh->GetMaterialIndex(meshIdx);

			m_ObjBuffer->LoadData(GetMainContext(), objData);

			m_ColorPass->GetOpaquePipelineState().CommitResources(GetMainContext(), m_Binder.get());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_Mesh->GetIndexBuffer(meshIdx)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_Mesh->GetVertexBuffer(meshIdx)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexCount(meshIdx), 1, 0, 0, 0);
		}

		m_Skybox->Render(GetMainContext(), GetCamera());

		m_ColorPass->GetTransmissionPipelineState().BeginPSO(GetMainContext());
		for (uint32 i = 0; i < m_Mesh->GetTransmissionMeshCount(); i++)
		{
			uint32 meshIdx = m_Mesh->GetTransmissionMeshIndex(i);

			PBRLighting::ObjectConstants objData;

			objData.World = (SimpleMath::Matrix::CreateScale(m_MeshScale) *
				SimpleMath::Matrix::CreateRotationX(XMConvertToRadians(m_MeshRotation.x)) *
				SimpleMath::Matrix::CreateRotationY(XMConvertToRadians(m_MeshRotation.y)) *
				SimpleMath::Matrix::CreateRotationZ(XMConvertToRadians(m_MeshRotation.z))).Transpose();

			objData.AlbedoTexIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_BASE_COLOR));
			objData.NormalMapIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_NORMAL_MAP));
			objData.ORMTexIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_ORM));
			objData.TransmissionTexIdx = GetTexIdx(m_Mesh->GetTexture(meshIdx, PBR_TEXTURE_TRANSMISSION));
			objData.ObjMaterialIdx = m_Mesh->GetMaterialIndex(meshIdx);

			m_ObjBuffer->LoadData(GetMainContext(), objData);

			m_ColorPass->GetTransmissionPipelineState().CommitResources(GetMainContext(), m_Binder.get());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_Mesh->GetIndexBuffer(meshIdx)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_Mesh->GetVertexBuffer(meshIdx)->GetView());
			GetMainContext()->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_Mesh->GetIndexCount(meshIdx), 1, 0, 0, 0);
		}

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
			m_Skybox->RebuildSky(envMapPath, GetMainContext(), m_Prepass.get());
		}
	}

	void PBRDemo::RenderImGui(bool& genEnvMap, char* envMapFile)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		genEnvMap = false;

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(350, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);

		ImGui::Begin("Editor", nullptr,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize);

		static int currentModel = 0;

		if (ImGui::Combo("Model", &currentModel, Models, IM_ARRAYSIZE(Models)))
		{
			MeshLoadDesc meshDesc = {};
			meshDesc.Flags = MESH_LOAD_FLAG_LOAD_TEXTURES;
			meshDesc.TextureLoadDesc.Flags = TextureLoadDesc::CREATE_SRV;
			meshDesc.TextureLoadDesc.OnCPU = false;
			meshDesc.TextureBasePath = Textures[currentModel];
			meshDesc.TextureExt = ".dds";

			m_Mesh = std::make_shared<Mesh>(GetDevice(), GetMainContext(), Models[currentModel]);
			m_Mesh->Load(meshDesc);

			m_TextureIndexes.MaterialBufferIdx = m_Mesh->GetMaterials()->GetSRVView()->GetGpuHeapIndex();
			m_TextureIdxBuffer->LoadData(GetMainContext(), &m_TextureIndexes);

			m_Binder->DryMutableResources();
			m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbTextureIndexes", m_TextureIdxBuffer);
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
			ImGui::DragFloat("Scale", &m_MeshScale);
			ImGui::DragFloat3("Rotation", (float*)&m_MeshRotation, 1.0f, -180.0, 180.0);
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
				if (current == 0) m_Skybox->SetSky(m_Skybox->GetHDREnvCubeMap()->GetSRVView()->GetGpuHeapIndex());
				else if (current == 1) m_Skybox->SetSky(m_Skybox->GetIrradianceMap()->GetSRVView()->GetGpuHeapIndex());
				else if (current == 2) m_Skybox->SetSky(m_Skybox->GetPrefilteredMap()->GetSRVView()->GetGpuHeapIndex());
			}

			if (ImGui::SliderFloat("Lod", &lod, 0.0f, IBLRendering::PREFILTERED_MIP_LEVELS))
			{
				m_Skybox->SetLod(lod);
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

		RenderEngine::PopulateDebugImguiCommand();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetMainContext()->GetCommandCtx()->GetCmdList());
	}

	void PBRDemo::BuildPBRPass(const wchar_t* debugView)
	{
		LPCWSTR macros[]
		{
			debugView, L"1",
			NULL, NULL,
		};

		m_ColorPass = std::make_unique<PBRLighting>(GetDevice(), macros);
		m_ColorPass->Build(GetDevice());

		m_Binder = m_ColorPass->GetOpaquePipelineState().CreateShaderBinder();

		m_Binder->BindResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbTextureIndexes", m_TextureIdxBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "gLight", m_LightBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbPerObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_VERTEX, "cbPerPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EduEngine::EduBinding::EDU_SHADER_TYPE_PIXEL, "cbPerPass", m_PassBuffer);
	}
}