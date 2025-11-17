#pragma once

#include <memory>

#include "../../BaseDemo/include/RenderEngine.h"
#include "../../BaseDemo/include/Camera.h"
#include "../../BaseDemo/include/RenderPasses.h"
#include "../../BaseDemo/include/Texture.h"
#include "../../BaseDemo/include/Mesh.h"
#include "../../Graphics/include/DynamicUploadBuffer.h"

#include <DebugRendererSystem.h>

namespace EduEngine
{
	class PBRDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_LightBuffer;
		std::unique_ptr<PBRLighting> m_ColorPass;

		std::shared_ptr<Texture> m_AlbedoTexture;
		std::shared_ptr<Texture> m_MetallicRoughnessTexture;
		std::shared_ptr<Texture> m_AOTexture;
		std::shared_ptr<Texture> m_NormalMapTexture;
		std::shared_ptr<Mesh> m_Mesh;

		std::shared_ptr<DebugRendererSystem> m_DebugRenderer;
	
		PBRLighting::Light m_LightConstants;
	};
}