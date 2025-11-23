#include "pch.h"
#include "PipelineStateD3D12.h"

namespace EduEngine::DiligentBinding
{
	PipelineStateD3D12::PipelineStateD3D12() :
		PipelineStateD3D12Base(QueueID::Direct, false)
	{
		ZeroMemory(&m_Desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

		m_Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		m_Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		m_Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		m_Desc.SampleMask = UINT_MAX;
		m_Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		m_Desc.NumRenderTargets = 0;
		m_Desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		m_Desc.SampleDesc.Count = 1;
		m_Desc.SampleDesc.Quality = 0;
		m_Desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}

	void PipelineStateD3D12::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
	{
		m_Desc.PrimitiveTopologyType = topology;
	}

	void PipelineStateD3D12::SetBlendState(D3D12_BLEND_DESC blendState)
	{
		m_Desc.BlendState = blendState;
	}

	void PipelineStateD3D12::SetRTBlendState(int rtIndex, D3D12_RENDER_TARGET_BLEND_DESC blendState)
	{
		m_Desc.BlendState.RenderTarget[0] = blendState;
	}

	void PipelineStateD3D12::SetInputLayout(D3D12_INPUT_LAYOUT_DESC inputLayout)
	{
		m_Desc.InputLayout = inputLayout;
	}

	void PipelineStateD3D12::SetRTVFormat(const DXGI_FORMAT format)
	{
		m_Desc.NumRenderTargets = 1;
		m_Desc.RTVFormats[0] = format;
	}

	void PipelineStateD3D12::SetRTVFormats(int count, const DXGI_FORMAT* formats)
	{
		assert(count > 0);
		m_Desc.NumRenderTargets = count;

		for (int i = 0; i < count; i++)
			m_Desc.RTVFormats[i] = formats[i];
	}

	void PipelineStateD3D12::SetRasterizerState(D3D12_RASTERIZER_DESC rasterizerDesc)
	{
		m_Desc.RasterizerState = rasterizerDesc;
	}

	void PipelineStateD3D12::SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC depthStencilDesc)
	{
		m_Desc.DepthStencilState = depthStencilDesc;
	}

	void PipelineStateD3D12::SetDepthStencilFormat(DXGI_FORMAT format)
	{
		m_Desc.DSVFormat = format;
	}

	void PipelineStateD3D12::SetShader(const std::shared_ptr<ShaderD3D12>& shader)
	{
		switch (shader->GetShaderType())
		{
		case EDU_SHADER_TYPE_VERTEX:
			m_Desc.VS = shader->GetShaderBytecode();
			break;
		case EDU_SHADER_TYPE_GEOMETRY:
			m_Desc.GS = shader->GetShaderBytecode();
			break;
		case EDU_SHADER_TYPE_PIXEL:
			m_Desc.PS = shader->GetShaderBytecode();
			break;
		default:
			ASSERT_FAILED("Unexpected shader type");
		}

		SetShaderBase(shader);
	}

	void PipelineStateD3D12::BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso)
	{
		m_Desc.pRootSignature = rootSignature;

		HRESULT hr = device->CreateGraphicsPipelineState(&m_Desc, IID_PPV_ARGS(&pso));
		THROW_IF_FAILED(hr, L"Failed to create PSO");
	}
}