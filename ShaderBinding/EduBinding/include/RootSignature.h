#pragma once
#include "framework.h"
#include "ShaderD3D12.h"

#include <RenderDeviceD3D12.h>

namespace EduEngine::EduBinding
{
	class RootSignature
	{
	public:
		void Build(RenderDeviceD3D12* device,
				   ShaderD3D12** shaders,
				   uint8 shadersNum,
				   D3D12_STATIC_SAMPLER_DESC* overrideStaticSamplers = nullptr,
				   uint32 numOverrideStaticSamplers = 0);

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.Get(); }

#if defined(EDUBINDINGDEBUG)
		void DebugPrint();
#endif

	private:
		void InitStaticSamplers(D3D12_STATIC_SAMPLER_DESC* staticSamplers);

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

#if defined(EDUBINDINGDEBUG)
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
		D3D12_ROOT_PARAMETER1 params[64];
		std::vector<D3D12_DESCRIPTOR_RANGE1> tableRanges[SHADER_RESOURCE_TYPE_NUM][EDU_SHADER_TYPE_NUM];
		CD3DX12_STATIC_SAMPLER_DESC staticSamplers[16];
#endif
	};
}