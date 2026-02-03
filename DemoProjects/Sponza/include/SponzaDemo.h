#pragma once

#include <RenderEngine.h>
#include <Camera.h>
#include <PipelineState.h>
#include <BufferD3D12.h>
#include <SSAO.h>
#include <GBuffer.h>
#include <PBRPrepass.h>
#include <DeferredPBRLightPass.h>
#include <CSMRendering.h>
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
		void BuildDrawPso();
		std::shared_ptr<PipelineStateBase> BuildPostProcPso();

	private:
		std::unique_ptr<PBRPrepass> m_PbrPrepass;
		std::unique_ptr<DeferredPBRLightPass> m_LightPass;
		std::unique_ptr<SSAO> m_Ssao;
		std::unique_ptr<GBuffer> m_GBuffer;
		std::unique_ptr<CSMRendering> m_CSMRendering;

		std::shared_ptr<Mesh> m_Mesh;

		PipelineState m_DrawPso;
		PSOEntry m_PostProcPso;
		std::shared_ptr<ShaderBinder> m_DrawBinder;
		std::shared_ptr<ShaderBinder> m_PostProcBinder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		DescriptorHeapAllocation m_GpuCopyDescriptors;

	private:
		DXGI_FORMAT SPONZA_G_BUFFERS[SponzaGBufferId::NumBuffers]
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
		};
	};
}