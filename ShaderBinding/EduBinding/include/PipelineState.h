#pragma once
#include "framework.h"
#include "ShaderBinder.h"
#include "RootSignature.h"

namespace EduEngine::EduBinding
{
	class EDUBINDING_API PipelineState
	{
	public:
		PipelineState();

		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology);
		void SetBlendState(D3D12_BLEND_DESC blendState);
		void SetRTBlendState(int rtIndex, D3D12_RENDER_TARGET_BLEND_DESC blendState);
		void SetInputLayout(D3D12_INPUT_LAYOUT_DESC inputLayout);
		void SetRTVFormat(const DXGI_FORMAT format);
		void SetRTVFormats(int count, const DXGI_FORMAT* formats);
		void SetRasterizerState(D3D12_RASTERIZER_DESC rasterizerDesc);
		void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC depthStencilDesc);
		void SetDepthStencilFormat(DXGI_FORMAT format);
		void SetShader(const std::shared_ptr<ShaderD3D12>& shader);

		void Build(RenderDeviceD3D12* device);
		void CommitAll(DeviceContext* context);
		
		IShaderBinder* GetShaderBinder() const { return m_ShaderBinder.get(); }

#ifdef _DEBUG
		void DebugPrint();
#endif

	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_Desc;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;

		std::shared_ptr<ShaderD3D12> m_Shaders[EDU_SHADER_TYPE_NUM];
		std::shared_ptr<ShaderBinder> m_ShaderBinder;
		RootSignature m_RootSignature;
	};
}