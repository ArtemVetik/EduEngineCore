#pragma once
#include "framework.h"
#include "PipelineStateBase.h"

namespace EduEngine::EduBinding
{
	class EDUBINDING_API ComputePipelineState : public PipelineStateBase
	{
	public:
		ComputePipelineState(QueueMask queueMask);

		void SetShader(const std::shared_ptr<ShaderD3D12>& shader);

		void BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) override;

	private:
		D3D12_COMPUTE_PIPELINE_STATE_DESC m_Desc;
	};
}