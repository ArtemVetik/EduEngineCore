#pragma once

#include <RenderEngine.h>
#include <Camera.h>
#include <PipelineState.h>
#include <BufferD3D12.h>
#include <DebugRendererSystem.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class TemporalAADemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;
		void OnResize() override;

	private:
		void LoadModel(const char* model, const char* texturePath);
		void BuildResolveConstantBuffer();

	private:
		std::shared_ptr<Mesh> m_Mesh;

		std::shared_ptr<TextureD3D12> m_CurrentTex;
		std::shared_ptr<TextureD3D12> m_HistoryTex[2];
		std::shared_ptr<TextureD3D12> m_MotionVectors;

		PipelineState m_DrawPso;
		PipelineState m_ResolvePso;
		PipelineState m_PostProcPso;
		PipelineState m_SkyboxPso;

		std::vector<std::shared_ptr<ShaderBinder>> m_DrawBinders;
		std::shared_ptr<ShaderBinder> m_ResolveBinder[2];
		std::shared_ptr<ShaderBinder> m_PostProcBinder[2];
		std::shared_ptr<ShaderBinder> m_SkyboxBinder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_SkyboxPassBuffer;

		std::shared_ptr<BufferD3D12> m_ResolveConstantsBuffer;

		std::shared_ptr<IndexBufferD3D12> m_CubeIB;
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;

		std::shared_ptr<DebugRendererSystem> m_DebugRenderer;

		uint32 m_ModelIdx = 0;
		uint32 m_VelocityMode = 0;
		bool m_Animate = false;
		float m_AnimateSpeed = 1.0f;
	};
}