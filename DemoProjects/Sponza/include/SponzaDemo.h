#pragma once

#include <RenderEngine.h>
#include <Camera.h>
#include <PipelineState.h>
#include <BufferD3D12.h>
#include <SSAO.h>
#include <GBuffer.h>
#include <PBRPrepass.h>
#include <DeferredPBRLightPass.h>
#include <SponzaCommon.h>

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
		std::unique_ptr<PBRPrepass> m_PbrPrepass;
		std::unique_ptr<DeferredPBRLightPass> m_LightPass;
		std::unique_ptr<SSAO> m_Ssao;
		std::unique_ptr<GBuffer> m_GBuffer;

		std::shared_ptr<Mesh> m_Mesh;

		PipelineState m_DrawPso;
		PipelineState m_PostProcPso;
		std::shared_ptr<ShaderBinder> m_DrawBinder;
		std::shared_ptr<ShaderBinder> m_PostProcBinder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		DescriptorHeapAllocation m_GpuCopyDescriptors;
	};
}