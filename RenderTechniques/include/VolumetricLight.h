#pragma once
#include "framework.h"
#include "PipelineState.h"
#include <TextureD3D12.h>
#include <Camera.h>
#include <CSMRendering.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API VolumetricLight
	{
	public:
		struct Settings
		{
			UINT NumSteps = 100;
			float Density = 0.6;
			float Absorption = 0.04f;
			float Intensity = 1.0f;
		};

	public:
		VolumetricLight(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height);

		void Render(DeviceContext* context, const Camera* camera, CSMRendering* csmRendering, Light* light, UINT depthTexId);
		void Resize(UINT width, UINT height);

		TextureD3D12* GetTexture() const { return m_LightTexture.get(); }

		Settings GetSettings() const { return m_Settings; }
		void UpdateSettings(Settings settings) { m_Settings = settings; }

	private:
		PipelineState m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::shared_ptr<TextureD3D12> m_LightTexture;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		Settings m_Settings;
		UINT m_Width = 0;
		UINT m_Height = 0;
		RenderDeviceD3D12* m_Device;
	};
}