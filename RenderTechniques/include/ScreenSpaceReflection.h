#pragma once
#include "framework.h"

#include <Camera.h>
#include <TextureD3D12.h>
#include <ComputePipelineState.h>
#include <RenderFeatures.h>
#include <BufferD3D12.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API ScreenSpaceReflection
	{
	public:
		struct Settings
		{
			UINT MaxIterations = 1024;
			float DepthThickness = 0.001f;
			bool BlurEnabled = false;
		};

		struct TextureIndexes
		{
			UINT ColorTexIdx;
			UINT NormalTexIdx;
			UINT MaskTexIdx;
			UINT DepthTexIdx;
		};

		ScreenSpaceReflection(RenderDeviceD3D12* device, DeviceContext* context, UINT rtWidth, UINT rtHeight);

		void UpdateIndexes(DeviceContext* context, TextureIndexes texIndexes);
		void UpdateSettings(DeviceContext* context, Settings settings);

		void Render(DeviceContext* context, const Camera* camera);
		void Resize(UINT width, UINT height);

		std::shared_ptr<TextureD3D12> GetSSRTextureShared() const;
		Settings GetSettings() const { return m_Settings; }

	private:
		std::shared_ptr<PipelineStateBase> BuildPso(bool blurPso);
		void UpdateConstantsBuffer(DeviceContext* context);

	private:
		PSOEntry m_MainPso;
		PSOEntry m_BlurPso;
		std::shared_ptr<ShaderBinder> m_MainBinder;
		std::shared_ptr<ShaderBinder> m_BlurBinder;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_BlurPassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;
		
		DescriptorHeapAllocation m_GpuHandles;
		std::shared_ptr<TextureD3D12> m_ReflectionTex[2];

		TextureIndexes m_TexIndexes = {};
		Settings m_Settings = {};
		D3D12_VIEWPORT m_Viewport = {};
		D3D12_RECT m_ScissorRect = {};

		RenderDeviceD3D12* m_Device;
	};
}