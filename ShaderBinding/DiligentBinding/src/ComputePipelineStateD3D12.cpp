#include "ComputePipelineStateD3D12.h"

namespace EduEngine::DiligentBinding
{
	ComputePipelineStateD3D12::ComputePipelineStateD3D12(QueueID queueId) :
		PipelineStateD3D12Base(queueId)
	{
		ZeroMemory(&m_Desc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
		m_Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	}

	void ComputePipelineStateD3D12::SetShader(const std::shared_ptr<ShaderD3D12>& shader)
	{
		VERIFY_EXPR(shader != nullptr, "");
		
		switch (shader->GetShaderType())
		{
		case EDU_SHADER_TYPE_COMPUTE:
			m_Desc.CS = shader->GetShaderBytecode();
			break;
		default:
			ASSERT_FAILED("Expected COMPUTE shader");
		}

		SetShaderBase(shader);
	}

	void ComputePipelineStateD3D12::BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso)
	{
		HRESULT hr = device->CreateComputePipelineState(&m_Desc, IID_PPV_ARGS(&pso));

		THROW_IF_FAILED(hr, L"Failed to create PSO");
	}
}