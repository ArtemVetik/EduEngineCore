#pragma once
#include "framework.h"

#include <ComputePipelineState.h>
#include <BufferD3D12.h>
#include <TextureD3D12.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	// This class implements the Inverse Fast Fourier Transform (IFFT) following https://doi.org/10.15480/882.1436.
	class RENDERTECHNIQUES_API IFFT
	{
	public:
		IFFT(RenderDeviceD3D12* device, DeviceContext* context, uint32 texturesSize, uint32 nbCascades, std::shared_ptr<BufferD3D12> constantBuffer);

		void InverseFastFourierTransform(TextureD3D12* inputTextureArray);

	private:
		std::shared_ptr<BufferD3D12> m_IFFTDataBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_IFFTPassBuffer;
		std::shared_ptr<TextureD3D12> m_TwiddleFactorsAndInputIndicesTexture;
		std::shared_ptr<TextureD3D12> m_InputTextures;
		std::shared_ptr<TextureD3D12> m_PingPongTextures;

		ComputePipelineState m_HorizontalStepIFFTPSO;
		ComputePipelineState m_VerticalStepIFFTPSO;
		ComputePipelineState m_PermutePSO;

		std::shared_ptr<ShaderBinder> m_HorizontalStepIFFTBinder;
		std::shared_ptr<ShaderBinder> m_VerticalStepIFFTBinder;
		std::shared_ptr<ShaderBinder> m_PermuteBinder;

		uint32 m_TexturesSize;

		DeviceContext* m_Context;
	};
}