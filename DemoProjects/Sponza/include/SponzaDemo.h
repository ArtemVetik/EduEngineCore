#pragma once

#include <RenderEngine.h>
#include <Camera.h>
#include <PipelineState.h>
#include <BufferD3D12.h>
#include <SSAO.h>
#include <GBuffer.h>
#include <IBLRendering.h>
#include <Skybox.h>
#include <DeferredPBRLightPass.h>
#include <CSMRendering.h>
#include <ReflectionProbe.h>
#include <ScreenSpaceReflection.h>
#include <VolumetricLight.h>
#include <Bloom.h>
#include <HZBGenerator.h>
#include <SponzaCommon.h>
#include <DebugRendererSystem.h>
#include <SponzaGUI.h>

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
		void UpdatePostProcData();
		void BuildDrawPso();
		std::shared_ptr<PipelineStateBase> BuildPostProcPso();

	private:
		std::unique_ptr<IBLRendering> m_IBLRendering;
		std::unique_ptr<Skybox> m_Skybox;
		std::unique_ptr<DeferredPBRLightPass> m_LightPass;
		std::unique_ptr<SSAO> m_Ssao;
		std::unique_ptr<GBuffer> m_GBuffer;
		std::unique_ptr<CSMRendering> m_CSMRendering;
		std::unique_ptr<ReflectionProbesManager> m_ReflectionProbeMgr;
		std::unique_ptr<ScreenSpaceReflection> m_SSR;
		std::unique_ptr<VolumetricLight> m_VolumetricLight;
		std::unique_ptr<Bloom> m_Bloom;
		std::unique_ptr<HZBGenerator> m_HZBGenerator;

		std::shared_ptr<Mesh> m_Mesh;

		PipelineState m_DrawPso;
		PSOEntry m_PostProcPso;
		std::shared_ptr<ShaderBinder> m_DrawBinder;
		std::shared_ptr<ShaderBinder> m_PostProcBinder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_PostProcBuffer;

		DescriptorHeapAllocation m_DepthGPUHandle;
		Light m_LightData = {};

		std::vector<RenderObject> m_RenderObjects;

		std::unique_ptr<DebugRendererSystem> m_DebugRenderer;

		SponzaGUI m_GUI;
		float m_SSRIntensity = 1.0f;
		bool m_SSREnabled = false;
		bool m_VolumetricLightEnabled = true;

	private:
		friend SponzaGUI;

		DXGI_FORMAT SPONZA_G_BUFFERS[SponzaGBufferId::NumBuffers]
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
		};
	};
}