#pragma once
#include "framework.h"
#include "IFFT.h"

#include <PipelineState.h>
#include <MeshGenerator.h>
#include <Camera.h>

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

	struct FFTOceanComputeSettings
	{
		float WindSpeed = 8;
		float WindDirectionX = 1;
		float WindDirectionY = -1;
		float Gravity = 9.81f;
		float Fetch = 50000;
		float Depth = 2560;
	};

	struct FFTOceanDrawSettings
	{
		UINT MaxLODLevel = 8;
		UINT TesselationLevel = 60;
		float MaxTesselationDistance = 10000;
		float TesselationDecayFactor = 10;
		float CullingTollerance = 10;
		float EnvironmentReflectionStrength = 1.f;
		XMFLOAT3 SubsurfaceScatteringColor = { 0.f, 1.f, 0.81f };
		float SubsurfaceScatteringIntensity = 0.02f;
		XMFLOAT3 DeepWaterColor = { 0.f, 0.1f, 0.4f };
		float WaterFogDensity = 0.22f;
		float RefractionStrength = 0.25f;
		float Roughness = 0.08f;
		float AnisoEX = 0.42f;
		float AnisoEY = 1.f;
		float FoamBlending = 0.f;
		float FoamThreshold = 0.f;
		XMFLOAT3 FoamColor = { 1.f, 1.f, 1.f };
		XMFLOAT3 ShadowsColor = { 0.f, 0.f, 0.f };
		float ShadowsIntensity = 0.34f;
		float SunReflectionStrength = 1.f;
	};

	class RENDERTECHNIQUES_API FFTOcean
	{
	public:
		static constexpr uint32 MaxCascades = 8;

		struct InitialSettings
		{
			TextureD3D12* AtmosphereCube = nullptr;
			uint32 TextureSize = 512;
			uint32 CascadesCount = 3;
			WaterCascade Cascades[MaxCascades];
		};

	public:
		FFTOcean(RenderDeviceD3D12* device, DeviceContext* context, InitialSettings initialSettings);

		void Compute(float time);
		void Render(Camera* camera, XMFLOAT3 sunPos, XMFLOAT3 sunColor);

		void UpdateComputeSettings(FFTOceanComputeSettings newSettings);
		void UpdateDrawSettings(FFTOceanDrawSettings newSettings);

		FFTOceanComputeSettings GetComputeSettings() const { return m_ComputeSettings; }
		FFTOceanDrawSettings GetDrawSettings() const { return m_DrawSettings; }

	private:
		void CalculateInitialSpectrum();
		void GenerateRandomNoiseTexture(RenderDeviceD3D12* device, DeviceContext* context);

	private:
		std::shared_ptr<BufferD3D12> m_ComputeConstantsBuffer;
		std::shared_ptr<BufferD3D12> m_DrawConstantsBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		std::unique_ptr<MeshGenerator> m_MeshGenerator;

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

		// PSO & Binders
		PipelineState m_DrawPSO;
		std::unique_ptr<ComputePipelineState> m_InitialSpectrumTexturesPSO;
		std::unique_ptr<ComputePipelineState> m_ConjugatedInitialSpectrumTexturesPSO;
		std::unique_ptr<ComputePipelineState> m_TimeDependentComplexAmplitudesAndDerivativesPSO;
		std::unique_ptr<ComputePipelineState> m_FillResultTexturesPSO;

		std::shared_ptr<ShaderBinder> m_DrawBinder;
		std::shared_ptr<ShaderBinder> m_InitialSpectrumTexturesBinder;
		std::shared_ptr<ShaderBinder> m_ConjugatedInitialSpectrumTexturesBinder;
		std::shared_ptr<ShaderBinder> m_TimeDependentComplexAmplitudesAndDerivativesBinder;
		std::shared_ptr<ShaderBinder> m_FillResultTexturesBinder;

		TextureD3D12* m_AtmosphereCube;
		uint32 m_CascadesCount;
		uint32 m_TextureSize;
		FFTOceanComputeSettings m_ComputeSettings;
		FFTOceanDrawSettings m_DrawSettings;
		DeviceContext* m_Context;
	};
}