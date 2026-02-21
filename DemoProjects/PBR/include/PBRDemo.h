#pragma once

#include <memory>

#include "../../BaseDemo/include/RenderEngine.h"
#include "../../BaseDemo/include/RenderPasses.h"
#include "../../Graphics/include/DynamicUploadBuffer.h"

#include <Mesh.h>
#include <DebugRendererSystem.h>
#include <Camera.h>
#include <IBLRendering.h>
#include <Skybox.h>

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
		std::shared_ptr<BufferD3D12> m_TextureIdxBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_LightBuffer;
		std::unique_ptr<PBRLighting> m_ColorPass;
		std::shared_ptr<EduEngine::EduBinding::ShaderBinder> m_Binder;

		std::shared_ptr<Mesh> m_Mesh;

		std::shared_ptr<IBLRendering> m_Prepass;
		std::shared_ptr<Skybox> m_Skybox;
		std::shared_ptr<DebugRendererSystem> m_DebugRenderer;
	
		float m_MeshScale;
		XMFLOAT3 m_MeshRotation;
		PBRLighting::MaterialConstants m_MaterialConstants;
		PBRLighting::TextureIndexes m_TextureIndexes;
		PBRLighting::Light m_LightConstants;
	};
}