#pragma once

#include <RenderEngine.h>
#include <Camera.h>
#include <PipelineState.h>
#include <BufferD3D12.h>
#include <SSAO.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class SponzaDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;
		void OnResize() override;

	private:
		std::unique_ptr<SSAO> m_Ssao;
		std::shared_ptr<Mesh> m_Mesh;

		PipelineState m_DrawPso;
		PipelineState m_PostProcPso;
		std::vector<std::shared_ptr<ShaderBinder>> m_DrawBinders;
		std::shared_ptr<ShaderBinder> m_PostProcBinder;

		std::shared_ptr<TextureD3D12> m_ColorRT;
		std::shared_ptr<TextureD3D12> m_NormalRT;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
	};
}