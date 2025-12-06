#pragma once

#include <memory>

#include "../../BaseDemo/include/RenderEngine.h"
#include "../../BaseDemo/include/RenderPasses.h"
#include "../../BaseDemo/include/Texture.h"
#include "../../BaseDemo/include/Mesh.h"
#include "../../Graphics/include/DynamicUploadBuffer.h"

#include <DebugRendererSystem.h>
#include <Camera.h>
#include <PBRPrepass.h>

namespace EduEngine
{
	class PBRDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		void RenderImGui(bool& genEnvMap, char* envMapFile);
		void BuildPBRPass(const wchar_t* debugView = nullptr);

	private:
		std::shared_ptr<BufferD3D12> m_MaterialBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_LightBuffer;
		std::unique_ptr<PBRLighting> m_ColorPass;
		std::shared_ptr<EduEngine::EduBinding::ShaderBinder> m_Binder;
		bool m_PBRTextured;

		Texture m_AlbedoTexture;
		Texture m_MetallicRoughnessTexture;
		Texture m_AOTexture;
		Texture m_NormalMapTexture;
		std::shared_ptr<Mesh> m_Mesh;

		std::shared_ptr<PBRPrepass> m_Prepass;
		std::shared_ptr<DebugRendererSystem> m_DebugRenderer;
	
		float m_MeshScale;
		XMFLOAT3 m_MeshRotation;
		PBRLighting::MaterialConstants m_MaterialConstants;
		PBRLighting::Light m_LightConstants;
	};
}