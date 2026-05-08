#pragma once

#include "MeshGenerator.h"

#include <RenderEngine.h>
#include <PipelineState.h>

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
	};

	class FFTOceanDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		PipelineState m_DrawPSO;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;
		std::shared_ptr<ShaderBinder> m_DrawBinder;

		std::unique_ptr<MeshGenerator> m_MeshGenerator;

		ConstantsData m_ConstantsData;
	};
}