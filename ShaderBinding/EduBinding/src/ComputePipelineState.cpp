#include "ComputePipelineState.h"

namespace EduEngine::EduBinding
{
	ComputePipelineState::ComputePipelineState(QueueID queueId) :
		PipelineStateBase(queueId, true)
	{
		ZeroMemory(&m_Desc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
		m_Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	}

	void ComputePipelineState::SetShader(const std::shared_ptr<ShaderD3D12>& shader)
	{
		VERIFY_EXPR(shader != nullptr, "");

		switch (shader->GetType())
		{
		case EDU_SHADER_TYPE_COMPUTE:
			m_Desc.CS = shader->GetShaderBytecode();
			break;
		default:
			ASSERT_FAILED("Expected COMPUTE shader");
		}

		SetShaderBase(shader);
	}

	void ComputePipelineState::BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso)
	{
		m_Desc.pRootSignature = rootSignature;

		HRESULT hr = device->CreateComputePipelineState(&m_Desc, IID_PPV_ARGS(&pso));
		THROW_IF_FAILED(hr, L"Failed to create PSO");
	}
}