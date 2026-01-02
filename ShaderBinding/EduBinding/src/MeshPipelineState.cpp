#include "MeshPipelineState.h"

namespace EduEngine::EduBinding
{
	MeshPipelineState::MeshPipelineState() :
		PipelineStateBase(QueueId::Direct, false)
	{
		m_Desc = {};
		m_Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		m_Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		m_Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		m_Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		m_Desc.SampleMask = UINT_MAX;
		m_Desc.NumRenderTargets = 0;
		m_Desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		m_Desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		m_Desc.SampleDesc.Count = 1;
		m_Desc.SampleDesc.Quality = 0;
	}

	void MeshPipelineState::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
	{
		m_Desc.PrimitiveTopologyType = topology;
	}

	void MeshPipelineState::SetBlendState(D3D12_BLEND_DESC blendState)
	{
		m_Desc.BlendState = blendState;
	}

	void MeshPipelineState::SetRTBlendState(int rtIndex, D3D12_RENDER_TARGET_BLEND_DESC blendState)
	{
		m_Desc.BlendState.RenderTarget[0] = blendState;
	}

	void MeshPipelineState::SetRTVFormat(const DXGI_FORMAT format)
	{
		m_Desc.NumRenderTargets = 1;
		m_Desc.RTVFormats[0] = format;
	}

	void MeshPipelineState::SetRTVFormats(int count, const DXGI_FORMAT* formats)
	{
		VERIFY_EXPR(count > 0, "RTV count must be non-zero");
		m_Desc.NumRenderTargets = count;

		for (int i = 0; i < count; i++)
			m_Desc.RTVFormats[i] = formats[i];
	}

	void MeshPipelineState::SetRasterizerState(D3D12_RASTERIZER_DESC rasterizerDesc)
	{
		m_Desc.RasterizerState = rasterizerDesc;
	}

	void MeshPipelineState::SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC depthStencilDesc)
	{
		m_Desc.DepthStencilState = depthStencilDesc;
	}

	void MeshPipelineState::SetDepthStencilFormat(DXGI_FORMAT format)
	{
		m_Desc.DSVFormat = format;
	}

	void MeshPipelineState::SetShader(const std::shared_ptr<ShaderD3D12>& shader)
	{
		switch (shader->GetType())
		{
		case EDU_SHADER_TYPE_AMPLIFICATION:
			m_Desc.AS = shader->GetShaderBytecode();
			break;
		case EDU_SHADER_TYPE_MESH:
			m_Desc.MS = shader->GetShaderBytecode();
			break;
		case EDU_SHADER_TYPE_PIXEL:
			m_Desc.PS = shader->GetShaderBytecode();
			break;
		default:
			ASSERT_FAILED("Unexpected shader type");
		}

		SetShaderBase(shader);
	}

	void MeshPipelineState::BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso)
	{
		ID3D12Device2* device2 = static_cast<ID3D12Device2*>(device);

		m_Desc.pRootSignature = rootSignature;

		auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(m_Desc);

		D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
		streamDesc.pPipelineStateSubobjectStream = &psoStream;
		streamDesc.SizeInBytes = sizeof(psoStream);

		HRESULT hr = device2->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pso));
		THROW_IF_FAILED(hr, L"Failed to create PSO");

		pso->SetName(L"MESH_PSO");
	}
}