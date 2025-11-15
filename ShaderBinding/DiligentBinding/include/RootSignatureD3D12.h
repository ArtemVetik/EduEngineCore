#pragma once
#include "framework.h"
#include "ShaderAPI.h"
#include <RenderDeviceD3D12.h>
#include "RootParamsManager.h"

#include "ShaderD3D12.h"
#include "ShaderResourceLayoutD3D12.h"

namespace EduEngine::DiligentBinding
{
	class RootSignatureD3D12
	{
	public:
		RootSignatureD3D12();

		void AllocateResourceSlot(EDU_SHADER_TYPE			   shaderType,
								  const ShaderResourceAttribs& shaderResAttribs,
								  D3D12_DESCRIPTOR_RANGE_TYPE  rangeType,
								  uint32&					   rootIndexOut,
								  uint32&					   offsetFromTableStartOut);

		void AllocateStaticSamplers(const ShaderD3D12** shaders, uint32 numShaders);
		void InitStaticSampler(EDU_SHADER_TYPE shaderType, const String& textureName, const ShaderResourceAttribs& samplerAttribs);

		void CommitRootViews(ShaderResourceCacheD3D12& resourceCache,
							 class CommandContext* ctx,
							 bool isCompute) const;

		void CommitDescriptorHandles(RenderDeviceD3D12* device,
									 ShaderResourceCacheD3D12& resourceCache,
									 class CommandContext* ctx,
									 bool isCompute,
									 bool transitionResources) const;

		void Build(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache);

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_d3d12RootSignature.Get(); }

#ifdef _DEBUG
		void DebugPrint();
#endif

	private:
		void InitResourceCache(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache);

		void CommitDescriptorHandlesInternal_SM(RenderDeviceD3D12* pRenderDeviceD3D12,
												ShaderResourceCacheD3D12& ResourceCache,
												class CommandContext* Ctx,
												bool IsCompute,
												bool transitionResources) const;

		void CommitDescriptorHandlesInternal_SMD(RenderDeviceD3D12* pRenderDeviceD3D12,
												 ShaderResourceCacheD3D12& ResourceCache,
												 class CommandContext* Ctx,
												 bool IsCompute,
												 bool transitionResources) const;

	private:
		bool m_DynamicSignature;

		uint32 m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_NUM_TYPES];
		uint32 m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_NUM_TYPES];

		static const uint8 InvalidRootTableIndex = static_cast<uint8>(-1);

		// ( [STATIC / MUTABLE] | [DYNAMIC] ) * ( EDU_SHADER_TYPE_NUM_TYPES )
		uint8 m_SrvCbvUavRootTablesMap[2 * EDU_SHADER_TYPE_NUM_TYPES];
		uint8 m_SamplerRootTablesMap[2 * EDU_SHADER_TYPE_NUM_TYPES];

		struct StaticSamplerAttribs
		{
			StaticSamplerDesc SamplerDesc;
			UINT ArraySize = 0;

			StaticSamplerAttribs() {}
			StaticSamplerAttribs(const StaticSamplerDesc& samDesc, D3D12_SHADER_VISIBILITY visibility) :
				SamplerDesc(samDesc)
			{
				SamplerDesc.Desc.ShaderVisibility = visibility;
			}
		};

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		std::vector<D3D12_ROOT_PARAMETER> m_RootParamsD3D12;
		std::vector<D3D12_STATIC_SAMPLER_DESC> m_d3d12StaticSamplers;

		std::vector<StaticSamplerAttribs> m_StaticSamplers;
		RootParamsManager m_RootParams;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_d3d12RootSignature;
	};
}