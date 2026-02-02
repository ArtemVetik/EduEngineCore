#pragma once
#include <Camera.h>
#include <TextureD3D12.h>
#include <PipelineState.h>
#include <RenderFeatures.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API SSAO
	{
	public:
		SSAO(RenderDeviceD3D12* device, DeviceContext* context, uint64 rtWidth, uint64 rtHeight);

		void BindResources(std::shared_ptr<TextureD3D12> normalMap, std::shared_ptr<TextureD3D12> depthMap);

		void Update(const Camera* camera, DeviceContext* context);
		void Render(DeviceContext* context);

		void Resize(uint64 rtWidth, uint64 rtHeight);

		std::shared_ptr<TextureD3D12> GetSSAOMap() const { return m_SsaoTexture[0]; }

	private:
		std::shared_ptr<PipelineState> BuildPSO(bool blurPso);
		std::vector<float> CalcGaussWeights(float sigma);

	private:
		const DXGI_FORMAT SSAO_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
		const int MAX_BLUR_RADIUS = 5;

		PSOEntry m_SsaoPsoEntry = {};
		PSOEntry m_BlurPsoEntry = {};

		std::shared_ptr<ShaderBinder> m_SsaoBinder;
		std::shared_ptr<ShaderBinder> m_BlurBinder[2];

		std::shared_ptr<TextureD3D12> m_SsaoTexture[2];
		std::shared_ptr<TextureD3D12> m_RandVectorMap;

		std::shared_ptr<DynamicUploadBuffer> m_SsaoBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_ConstantsBuffer;

		XMFLOAT4 m_Offsets[14];
		uint64 m_Width;
		uint64 m_Height;
		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		RenderDeviceD3D12* m_Device;
	};
}