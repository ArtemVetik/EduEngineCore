#pragma once

#include "MeshGenerator.h"

#include <RenderEngine.h>
#include <FFTOcean.h>
#include <Atmosphere.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
	{
		UINT MaxLODLevel = 8;
		UINT TesselationLevel = 10;
		float MaxTesselationDistance = 350;
		float TesselationDecayFactor = 8;
		float CullingTollerance = 6;

		UINT NbCascades = 1;
		UINT WavelengthsIdx;
		UINT DisplacementsTexturesIdx;
		UINT DerivativesTexturesIdx;
		UINT TurbulenceTexturesIdx;

		float EnvironmentReflectionStrength = 1.f;
		UINT Padding;
		XMFLOAT3 SubsurfaceScatteringColor = { 0.f, 1.f, 0.8f };
		float SubsurfaceScatteringIntensity = 0.07f;
		XMFLOAT3 DeepWaterColor = { 0.f, 0.1f, 0.4f };
		float WaterFogDensity = 0.22f;
		float RefractionStrength = 0.25f;
		float Roughness = 0.08f;
		float AnisoEX = 0.42f;
		float AnisoEY = 1.f;
		float FoamBlending = 0.f;
		float FoamThreshold = 0.f;
		XMFLOAT2 FoamPadRow = {};
		XMFLOAT3 FoamColor = { 1.f, 1.f, 1.f };
		float FoamPad0 = 0.f;
		XMFLOAT3 ShadowsColor = { 0.f, 0.f, 0.f };
		float ShadowsIntensity = 0.34f;
		float SunReflectionStrength = 1.f;
		XMFLOAT3 DrawConstantsPad1 = {};
		XMFLOAT4 DrawConstantsPad2[5] = {};
	};
	static_assert(sizeof(ConstantsData) == 256, "ConstantsData must match FFTOceanDraw.hlsl cbConstants size (256 bytes)");

	class FFTOceanDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		void RenderGui();
		void RefreshOceanGpuConstants();
		void RecreateFFTOceanPreservingSettings();
		void FillMainLightPosFromSunAngles(XMFLOAT3& outMainLightPos) const;

	private:
		PipelineState m_DrawPSO;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;
		std::shared_ptr<ShaderBinder> m_DrawBinder;

		std::unique_ptr<MeshGenerator> m_MeshGenerator;
		std::unique_ptr<FFTOcean> m_FFTOcean;
		std::unique_ptr<Atmosphere> m_Atmosphere;

		FFTOcean::InitialSettings m_OceanInitialSettings{};
		ConstantsData m_ConstantsData;

		float m_SunAzimuthDegrees = 84.29f;
		float m_SunElevationDegrees = 5.69f;
	};
}