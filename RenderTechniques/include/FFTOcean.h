#pragma once
#include "framework.h"
#include "IFFT.h"

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct WaterCascade
	{
		// Reference wavelength value for this cascade's waves.
		// It establishes the general scale of the waves.
		// https://en.wikipedia.org/wiki/Wavelength
		float Wavelength = 1530.0f;

		// The upper limit of the angular wavenumber (k) for waves considered in the initial spectrum computation for this cascade.
		// A higher value allows for the inclusion of higher-frequency (shorter wavelength) waves.
		// https://en.wikipedia.org/wiki/Wavenumber
		float CutoffHigh = 100000.0f;

		// The lower limit of the angular wavenumber (k) for waves considered in the initial spectrum computation for this cascade.
		// A lower value allows for the inclusion of lower-frequency (longer wavelength) waves.
		// https://en.wikipedia.org/wiki/Wavenumber
		float CutoffLow = 0.0001f;

		float Swell = 0.4f;
		float Fade = 0.1f;
	};

	class RENDERTECHNIQUES_API FFTOcean
	{
	public:
		static constexpr uint32 MaxCascades = 8;

		struct InitialSettings
		{
			uint32 TextureSize = 512;
			uint32 CascadesCount = 3;
			WaterCascade Cascades[MaxCascades];
		};

		struct Settings
		{
			float WindSpeed = 8;
			float WindDirectionX = 1;
			float WindDirectionY = -1;
			float Gravity = 9.81f;
			float Fetch = 50000;
			float Depth = 2560;
		};

	public:
		FFTOcean(RenderDeviceD3D12* device, DeviceContext* context, InitialSettings initialSettings);

		void Update(float time);

		void UpdateSettings(Settings newSettings);
		Settings GetSettings() const { return m_Settings; }

		UINT GetWaveLengthSrvGpuBufferIdx() const { return m_Wavelengths->GetSRVView()->GetGpuHeapIndex(); }
		UINT GetDisplacementsTexSrvGpuBufferIdx() const { return m_DisplacementsTextures->GetSRVView()->GetGpuHeapIndex(); }
		UINT GetDerivativesTexSrvGpuBufferIdx() const { return m_DerivativesTextures->GetSRVView()->GetGpuHeapIndex(); }
		UINT GetTurbulenceTexSrvGpuBufferIdx() const { return m_TurbulenceTextures->GetSRVView()->GetGpuHeapIndex(); }

	private:
		void CalculateInitialSpectrum(Settings settings);
		void GenerateRandomNoiseTexture(RenderDeviceD3D12* device, DeviceContext* context);

	private:
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;

		// IFFT.hlsl
		std::unique_ptr<IFFT> m_IFFT;

		// InitialSpectrum.hlsl
		std::shared_ptr<BufferD3D12> m_InitSpectrumBuffer;
		std::shared_ptr<TextureD3D12> m_RandomNoiseTexture;
		std::shared_ptr<TextureD3D12> m_InitialSpectrumTextures;
		std::shared_ptr<TextureD3D12> m_WavesDataTextures;
		std::shared_ptr<BufferD3D12> m_Wavelengths;
		std::shared_ptr<BufferD3D12> m_Cutoffs;
		std::shared_ptr<BufferD3D12> m_Fades;
		std::shared_ptr<BufferD3D12> m_Swells;

		// ResultTexturesFiller.hlsl
		std::shared_ptr<BufferD3D12> m_ResultTexturesBuffer;
		std::shared_ptr<TextureD3D12> m_DisplacementsTextures;
		std::shared_ptr<TextureD3D12> m_DerivativesTextures;
		std::shared_ptr<TextureD3D12> m_TurbulenceTextures;
		std::shared_ptr<TextureD3D12> m_DxDzTextures;
		std::shared_ptr<TextureD3D12> m_DyDxzTextures;
		std::shared_ptr<TextureD3D12> m_DyxDyzTextures;
		std::shared_ptr<TextureD3D12> m_DxxDzzTextures;

		// TimeDependentSpectrum.hlsl
		std::shared_ptr<BufferD3D12> m_TimeDependentBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_TimeBuffer;

		// PSO & Binders
		std::unique_ptr<ComputePipelineState> m_InitialSpectrumTexturesPSO;
		std::unique_ptr<ComputePipelineState> m_ConjugatedInitialSpectrumTexturesPSO;
		std::unique_ptr<ComputePipelineState> m_TimeDependentComplexAmplitudesAndDerivativesPSO;
		std::unique_ptr<ComputePipelineState> m_FillResultTexturesPSO;

		std::shared_ptr<ShaderBinder> m_InitialSpectrumTexturesBinder;
		std::shared_ptr<ShaderBinder> m_ConjugatedInitialSpectrumTexturesBinder;
		std::shared_ptr<ShaderBinder> m_TimeDependentComplexAmplitudesAndDerivativesBinder;
		std::shared_ptr<ShaderBinder> m_FillResultTexturesBinder;

		uint32 m_NbCascades;
		uint32 m_TextureSize;
		Settings m_Settings;
		DeviceContext* m_Context;
	};
}