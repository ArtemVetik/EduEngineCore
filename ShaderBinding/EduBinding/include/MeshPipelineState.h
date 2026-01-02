#pragma once
#include "framework.h"
#include "PipelineStateBase.h"

#include <d3dx12.h>

namespace EduEngine::EduBinding
{
	class EDUBINDING_API MeshPipelineState : public PipelineStateBase
	{
	public:
		MeshPipelineState();

		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology);
		void SetBlendState(D3D12_BLEND_DESC blendState);
		void SetRTBlendState(int rtIndex, D3D12_RENDER_TARGET_BLEND_DESC blendState);
		void SetRTVFormat(const DXGI_FORMAT format);
		void SetRTVFormats(int count, const DXGI_FORMAT* formats);
		void SetRasterizerState(D3D12_RASTERIZER_DESC rasterizerDesc);
		void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC depthStencilDesc);
		void SetDepthStencilFormat(DXGI_FORMAT format);

		void SetShader(const std::shared_ptr<ShaderD3D12>& shader);

	protected:
		void BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) override final;

	private:
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC m_Desc;
	};
}