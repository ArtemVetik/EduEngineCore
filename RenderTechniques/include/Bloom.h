#pragma once
#include "framework.h"
#include <TextureD3D12.h>
#include <BufferD3D12.h>
#include <PipelineState.h>
#include <SimpleMath.h>

using namespace EduEngine::EduBinding;
using namespace DirectX;

namespace EduEngine
{
	class RENDERTECHNIQUES_API Bloom
	{
	public:
		struct Settings
		{
			XMFLOAT3 Tint = { 1, 1, 1 };
			float Threshold = 2.2f;
			float Intensity = 1.0f;
			float Scatter = 0.6f;
		};

	public:
		Bloom(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height);

		void Render(DeviceContext* context, UINT inputTexIdx);
		void Resize(DeviceContext* context, UINT width, UINT height);

		UINT GetBloomTextureIdx() const { return m_BloomMipUp[0]->GetSRVView()->GetGpuHeapIndex(); }

		Settings GetSettings() { return m_Settings; }
		void UpdateSettings(DeviceContext* context, Settings settings);

	private:
		static constexpr int BloomMipCount = 6;

		void UpdateConstantsBuffer(DeviceContext* context);

	private:
		PipelineState m_ThresholdPso;
		PipelineState m_HPso;
		PipelineState m_VPso;
		PipelineState m_UpscalePso;

		std::shared_ptr<ShaderBinder> m_ThresholdBinder;
		std::shared_ptr<ShaderBinder> m_HBinder;
		std::shared_ptr<ShaderBinder> m_VBinder;
		std::shared_ptr<ShaderBinder> m_UpscaleBinder;

		std::unique_ptr<TextureD3D12> m_BloomMipDown[BloomMipCount];
		std::unique_ptr<TextureD3D12> m_BloomMipUp[BloomMipCount];
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;

		Settings m_Settings = {};
		int m_MipNum = 0;
		UINT m_Width = 0;
		UINT m_Height = 0;

		RenderDeviceD3D12* m_Device;
	};
}