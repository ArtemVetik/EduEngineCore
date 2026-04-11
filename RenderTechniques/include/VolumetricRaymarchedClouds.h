#pragma once
#include "framework.h"

#include <PipelineState.h>
#include <SwapChain.h>
#include <Texture.h>
#include <Timer.h>
#include <GenerateMipMaps.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API VolumetricRaymarchedClouds
	{
	public:
		VolumetricRaymarchedClouds(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height);

		void Render(DeviceContext* context, const SwapChain& swapChain, const Timer& timer);

		void Resize(UINT width, UINT height);

	private:
		PipelineState m_Pso;
		PipelineState m_PsoUpscale;
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<ShaderBinder> m_BinderUpscale;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::unique_ptr<Texture> m_NoiseTexture;
		std::unique_ptr<Texture> m_BlueNoiseTexture;
		std::shared_ptr<TextureD3D12> m_SceneTexture;

		GenerateMipMaps m_MipMapGen;
		UINT m_Width;
		UINT m_Height;
		UINT m_Frame;
		UINT m_DownScaleFactor = 1;
		RenderDeviceD3D12* m_Device;
	};
}