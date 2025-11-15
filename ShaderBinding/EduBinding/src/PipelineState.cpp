#include "PipelineState.h"
#include "ShaderBinder.h"
#include "RootSignature.h"

namespace EduEngine::EduBinding
{
	PipelineState::PipelineState()
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

	void PipelineState::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
	{
		m_Desc.PrimitiveTopologyType = topology;
	}

	void PipelineState::SetBlendState(D3D12_BLEND_DESC blendState)
	{
		m_Desc.BlendState = blendState;
	}

	void PipelineState::SetRTBlendState(int rtIndex, D3D12_RENDER_TARGET_BLEND_DESC blendState)
	{
		m_Desc.BlendState.RenderTarget[0] = blendState;
	}

	void PipelineState::SetInputLayout(D3D12_INPUT_LAYOUT_DESC inputLayout)
	{
		m_Desc.InputLayout = inputLayout;
	}

	void PipelineState::SetRTVFormat(const DXGI_FORMAT format)
	{
		m_Desc.NumRenderTargets = 1;
		m_Desc.RTVFormats[0] = format;
	}

	void PipelineState::SetRTVFormats(int count, const DXGI_FORMAT* formats)
	{
		VERIFY_EXPR(count > 0, "RTV count must be non-zero");
		m_Desc.NumRenderTargets = count;

		for (int i = 0; i < count; i++)
			m_Desc.RTVFormats[i] = formats[i];
	}

	void PipelineState::SetRasterizerState(D3D12_RASTERIZER_DESC rasterizerDesc)
	{
		m_Desc.RasterizerState = rasterizerDesc;
	}

	void PipelineState::SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC depthStencilDesc)
	{
		m_Desc.DepthStencilState = depthStencilDesc;
	}

	void PipelineState::SetDepthStencilFormat(DXGI_FORMAT format)
	{
		m_Desc.DSVFormat = format;
	}

	void PipelineState::SetShader(const std::shared_ptr<ShaderD3D12>& shader)
	{
		switch (shader->GetType())
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

		m_Shaders[shader->GetType()] = shader;
	}

	void PipelineState::Build(RenderDeviceD3D12* device)
	{
		uint8 shadersNum = 0;
		ShaderD3D12* activeShaders[EDU_SHADER_TYPE_NUM];

		for (uint8 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
		{
			if (m_Shaders[i].get())
			{
				activeShaders[shadersNum++] = m_Shaders[i].get();
			}
		}

		m_RootSignature.Build(device, activeShaders, shadersNum);

		m_ShaderBinder = std::make_shared<ShaderBinder>(device);
		m_ShaderBinder->Build(activeShaders, shadersNum);

		m_Desc.pRootSignature = m_RootSignature.GetD3D12RootSignature();

		HRESULT hr = device->GetD3D12Device()->CreateGraphicsPipelineState(&m_Desc, IID_PPV_ARGS(&m_PSO));
		THROW_IF_FAILED(hr, L"Failed to create PSO");
	}

	void PipelineState::CommitAll(DeviceContext* context)
	{
		context->GetCommandCtx()->GetCmdList()->SetPipelineState(m_PSO.Get());
		context->GetCommandCtx()->GetCmdList()->SetGraphicsRootSignature(m_RootSignature.GetD3D12RootSignature());

		m_ShaderBinder->CommitAll(context);
	}

#ifdef _DEBUG
	void PipelineState::DebugPrint()
	{
		for (uint8 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
		{
			if (m_Shaders[i].get())
			{
				m_Shaders[i]->DebugPrint();
			}
		}

		m_RootSignature.DebugPrint();
		m_ShaderBinder->DebugPrint();
	}
#endif
}