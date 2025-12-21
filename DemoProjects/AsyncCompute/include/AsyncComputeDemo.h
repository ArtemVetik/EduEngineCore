#pragma once

#include <RenderEngine.h>
#include <ComputePipelineState.h>
#include <PipelineState.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct ParticleData
	{
		XMFLOAT3 Pos;
		XMFLOAT3 Color;
		XMFLOAT3 Velocity;
		float Lifetime;
		float Age;
	};

	struct ComputePassCB
	{
		UINT MaxParticlesNum;
		float DeltaTime;
		float TotalTime;
		UINT EmitterSeed;
	};

	struct DrawPassCB
	{
		XMFLOAT4X4 ViewProj;
		float AspectRatio;
		XMUINT3 Padding = { 0, 0, 0 };
	};

	class AsyncComputeDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		static constexpr uint32 MaxParticles = 1000000;

		std::unique_ptr<ComputePipelineState> m_EmitPSO;
		std::unique_ptr<ComputePipelineState> m_UpdatePSO;
		std::unique_ptr<PipelineState> m_DrawPSO;

		std::shared_ptr<BufferD3D12> m_ParticlesBuffer;

		std::shared_ptr<ShaderBinder> m_EmitBinder;
		std::shared_ptr<ShaderBinder> m_UpdateBinder;
		std::shared_ptr<ShaderBinder> m_DrawBinder;

		std::shared_ptr<DynamicUploadBuffer> m_ComputePassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_DrawPassBuffer;
	};
}