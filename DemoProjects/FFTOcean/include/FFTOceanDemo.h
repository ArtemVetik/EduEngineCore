#pragma once

#include "MeshGenerator.h"

#include <RenderEngine.h>
#include <FFTOcean.h>
#include <Atmosphere.h>
#include <FFTOceanGUI.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
	{
		UINT MaxLODLevel = 8;
		UINT TesselationLevel = 60;
		float MaxTesselationDistance = 10000;
		float TesselationDecayFactor = 10;
		float CullingTollerance = 10;

		UINT NbCascades = 1;
		UINT WavelengthsIdx;
		UINT DisplacementsTexturesIdx;
		UINT DerivativesTexturesIdx;
		UINT TurbulenceTexturesIdx;
		UINT ReflectionCubeIdx;

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
		void OnResize() override;

	private:
		void RefreshOceanGpuConstants();
		void RecreateFFTOceanPreservingSettings();
		void FillMainLightPosFromSunAngles(XMFLOAT3& outMainLightPos) const;

	private:
		friend FFTOceanGUI;

		PipelineState m_DrawPSO;
		PipelineState m_PostProcPSO;

		std::shared_ptr<TextureD3D12> m_AccumulationBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;
		std::shared_ptr<ShaderBinder> m_DrawBinder;
		std::shared_ptr<ShaderBinder> m_PostProcBinder;

		std::unique_ptr<MeshGenerator> m_MeshGenerator;
		std::unique_ptr<FFTOcean> m_FFTOcean;
		std::unique_ptr<Atmosphere> m_Atmosphere;

		FFTOcean::InitialSettings m_OceanInitialSettings{};
		ConstantsData m_ConstantsData;
		FFTOceanGUI m_Gui;

		float m_SunAzimuthDegrees = 84.29f;
		float m_SunElevationDegrees = 5.69f;
	};
}