#pragma once
#include "framework.h"

#include <Camera.h>
#include <TextureD3D12.h>
#include <ComputePipelineState.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API ScreenSpaceReflection
	{
	public:
		struct Settings
		{
			UINT MaxIterations = 1024;
			float DepthThickness = 0.1f;
		};

		struct TextureIndexes
		{
			UINT AlbedoTexIdx;
			UINT NormalTexIdx;
			UINT MaskTexIdx;
			UINT DepthTexIdx;
			UINT OutTexIdx;
		};

		ScreenSpaceReflection(RenderDeviceD3D12* device, DeviceContext* context);

		void UpdateSettings(Settings settings) { m_Settings = settings; }
		void Setup(TextureIndexes texIndexes) { m_TexIndexes = texIndexes; }

		void Render(DeviceContext* context, const Camera* camera);

		Settings GetSettings() const { return m_Settings; }

	private:
		std::unique_ptr<ComputePipelineState> m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		TextureIndexes m_TexIndexes;
		Settings m_Settings = {};
	};
}