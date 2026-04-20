#pragma once
#include "framework.h"

#include <GenerateMipMaps.h>
#include <PipelineState.h>
#include <SwapChain.h>
#include <Texture.h>
#include <Timer.h>
#include <Camera.h>

using namespace EduEngine::EduBinding;
using namespace DirectX;

namespace EduEngine
{
	class RENDERTECHNIQUES_API VolumetricRaymarchedClouds
	{
	public:
		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
		{
			XMFLOAT3 SunPosition = { 1, 0, 0 };
			UINT MaxSteps = 100;
			float MarchSize = 0.16f;
		};

	public:
		VolumetricRaymarchedClouds(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height);

		void Update(DeviceContext* context, const Timer& timer, const Camera* camera);
		void Render(DeviceContext* context, const SwapChain& swapChain);

		void Resize(UINT width, UINT height);

		ConstantsData GetConstantsData() const { return m_ConstantsData; }
		void UpdateConstantsData(DeviceContext* context, ConstantsData& data);

	private:
		PipelineState m_Pso;
		PipelineState m_PsoUpscale;
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<ShaderBinder> m_BinderUpscale;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;

		std::unique_ptr<Texture> m_NoiseTexture;
		std::unique_ptr<Texture> m_BlueNoiseTexture;
		std::shared_ptr<TextureD3D12> m_SceneTexture;

		GenerateMipMaps m_MipMapGen;
		ConstantsData m_ConstantsData;
		UINT m_Width;
		UINT m_Height;
		UINT m_Frame;
		UINT m_DownScaleFactor = 1;

		RenderDeviceD3D12* m_Device;
	};
}