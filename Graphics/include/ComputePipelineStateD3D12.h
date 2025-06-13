#pragma once
#include "framework.h"
#include "PipelineStateD3D12Base.h"

namespace EduEngine
{
	class GRAPHICS_API ComputePipelineStateD3D12 : public PipelineStateD3D12Base
	{
	public:
		ComputePipelineStateD3D12(QueueID queueId);

		void SetShader(const std::shared_ptr<ShaderD3D12>& shader);

	protected:
		void BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) override;

	private:
		D3D12_COMPUTE_PIPELINE_STATE_DESC m_Desc;
	};
}