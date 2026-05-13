#pragma once

#include "MeshGenerator.h"

#include <RenderEngine.h>
#include <FFTOcean.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ConstantsData
	{
		UINT MaxLODLevel = 8;
		UINT TesselationLevel = 10;
		float MaxTesselationDistance = 250;
		float TesselationDecayFactor = 4;
		float CullingTollerance = 6;

		UINT NbCascades = 1;
		UINT WavelengthsIdx;
		UINT DisplacementsTexturesIdx;
	};

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

	private:
		PipelineState m_DrawPSO;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;
		std::shared_ptr<ShaderBinder> m_DrawBinder;

		std::unique_ptr<MeshGenerator> m_MeshGenerator;
		std::unique_ptr<FFTOcean> m_FFTOcean;

		FFTOcean::InitialSettings m_OceanInitialSettings{};
		ConstantsData m_ConstantsData;
	};
}