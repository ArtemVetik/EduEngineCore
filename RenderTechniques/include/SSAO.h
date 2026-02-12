#pragma once
#include <Camera.h>
#include <TextureD3D12.h>
#include <PipelineState.h>
#include <RenderFeatures.h>
#include <BufferD3D12.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API SSAO
	{
	public:
		struct Settings
		{
			float OcclusionRadius = 0.5f;
			float OcclusionFadeStart = 0.2f;
			float OcclusionFadeEnd = 1.0f;
			float SurfaceEpsilon = 0.05f;
		};

	public:
		SSAO(RenderDeviceD3D12* device, DeviceContext* context, uint64 rtWidth, uint64 rtHeight);

		void BindResources(DeviceContext* context, UINT normalTextureGpuIdx, UINT depthTextureGpuIdx);
		void UpdateSettings(DeviceContext* context, Settings settings);

		void Update(DeviceContext* context, const Camera* camera);
		void Render(DeviceContext* context);

		void Resize(uint64 rtWidth, uint64 rtHeight);

		Settings GetSettings() const { return m_Settings; }
		std::shared_ptr<TextureD3D12> GetSSAOMap() const { return m_SsaoTexture[0]; }

	private:
		void UpdateConstantsBuffer(DeviceContext* context);
		std::shared_ptr<PipelineState> BuildPSO(bool blurPso);
		std::vector<float> CalcGaussWeights(float sigma);

	private:
		const DXGI_FORMAT SSAO_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
		const int MAX_BLUR_RADIUS = 5;

		PSOEntry m_SsaoPsoEntry = {};
		PSOEntry m_BlurPsoEntry = {};

		std::shared_ptr<ShaderBinder> m_SsaoBinder;
		std::shared_ptr<ShaderBinder> m_BlurBinder;

		std::shared_ptr<TextureD3D12> m_SsaoTexture[2];
		std::shared_ptr<TextureD3D12> m_RandVectorMap;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_BlurPassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;

		XMFLOAT4 m_Offsets[14];
		UINT m_NormalTexIdx = -1;
		UINT m_DepthTexIdx = -1;

		D3D12_VIEWPORT m_Viewport = {};
		D3D12_RECT m_ScissorRect = {};
		Settings m_Settings = {};

		RenderDeviceD3D12* m_Device;
	};
}