#pragma once
#include "framework.h"
#include "ShaderAPI.h"

#include <vector>
#include <d3d12shader.h>

#include "StringUtils.h"

#include "../../Common/include/Asserts.h"

namespace EduEngine
{
	template<typename TOnResourcesCounted,
			 typename TOnNewCB,
			 typename TOnNewTexUAV,
			 typename TOnNewBufUAV,
			 typename TOnNewBufSRV,
			 typename TOnNewSampler,
			 typename TOnNewTexSRV>
	void LoadD3D12ShaderResource(ID3D12ShaderReflection* reflection,
								 TOnResourcesCounted OnResourcesCounted,
								 TOnNewCB OnNewCB,
								 TOnNewTexUAV OnNewTexUAV,
								 TOnNewBufUAV OnNewBufUAV,
								 TOnNewBufSRV OnNewBufSRV,
								 TOnNewSampler OnNewSampler,
								 TOnNewTexSRV OnNewTexSRV,
								 const ShaderDesc& shaderDesc)
	{
		UINT numCBs = 0, numTexSRVs = 0, numTexUAVs = 0, numBufSRVs = 0, numBufUAVs = 0, numSamplers = 0;

		D3D12_SHADER_DESC sDesc;
		reflection->GetDesc(&sDesc);

		std::vector<ShaderResourceAttribs> resources;
		resources.reserve(sDesc.BoundResources);

		for (UINT res = 0; res < sDesc.BoundResources; res += 1)
		{
			D3D12_SHADER_INPUT_BIND_DESC bindingDesc = {};
			reflection->GetResourceBindingDesc(res, &bindingDesc);

			SHADER_VARIABLE_TYPE varType = SHADER_VARIABLE_TYPE_NUM_TYPES;
			bool isStaticSampler = false;
			if (bindingDesc.Type == D3D_SIT_SAMPLER)
			{
				for (uint32 s = 0; s < shaderDesc.NumStaticSamplers; ++s)
				{
					if (StrCmpSuff(bindingDesc.Name, shaderDesc.StaticSamplers[s].TextureName, "_sampler")) // TODO: create suffix variable
					{
						isStaticSampler = true;
						break;
					}
				}

				varType = shaderDesc.DefaultVarType;

				for (UINT i = 0; i < shaderDesc.NumVarDesc; i++)
				{
					if (StrCmpSuff(bindingDesc.Name, shaderDesc.VarDesc[i].Name, "_sampler")) // TODO: create suffix variable
					{
						varType = shaderDesc.VarDesc[i].Type;
						break;
					}
				}
			}
			else
			{
				varType = GetShaderVariableType(bindingDesc.Name, shaderDesc);
			}

			switch (bindingDesc.Type)
			{
			case D3D_SIT_CBUFFER:                       ++numCBs; break;
			case D3D_SIT_TBUFFER:                       ASSERT_FAILED("TBuffers are not supported"); break;
			case D3D_SIT_TEXTURE:                       ++(bindingDesc.Dimension == D3D_SRV_DIMENSION_BUFFER ? numBufSRVs : numTexSRVs); break;
			case D3D_SIT_SAMPLER:                       ++numSamplers; break;
			case D3D_SIT_UAV_RWTYPED:                   ++(bindingDesc.Dimension == D3D_SRV_DIMENSION_BUFFER ? numBufUAVs : numTexUAVs); break;
			case D3D_SIT_STRUCTURED:                    ++numBufSRVs; break;
			case D3D_SIT_UAV_RWSTRUCTURED:              ++numBufUAVs; break;
			case D3D_SIT_BYTEADDRESS:                   ++numBufSRVs; break;
			case D3D_SIT_UAV_RWBYTEADDRESS:             ++numBufUAVs; break;
			case D3D_SIT_UAV_APPEND_STRUCTURED:         ASSERT_FAILED("Append structured buffers are not supported"); break;
			case D3D_SIT_UAV_CONSUME_STRUCTURED:        ASSERT_FAILED("Consume structured buffers are not supported"); break;
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: ASSERT_FAILED("RW structured buffers with counter are not supported"); break;
			default:									ASSERT_FAILED("Unexpected resource type");
			}

			resources.emplace_back(ShaderResourceAttribs(bindingDesc.Name, bindingDesc.BindPoint, bindingDesc.BindCount, bindingDesc.Type, varType, bindingDesc.Dimension, -1, isStaticSampler));
		}

		OnResourcesCounted(numCBs, numTexSRVs, numTexUAVs, numBufSRVs, numBufUAVs, numSamplers);

		std::vector<ShaderResourceAttribs> texSRVsRes;
		texSRVsRes.reserve(numTexSRVs);

		for (auto& res : resources)
		{
			switch (res.GetInputType())
			{
			case D3D_SIT_CBUFFER:
			{
				OnNewCB(std::move(res));
				break;
			}
			case D3D_SIT_TBUFFER:
			{
				ASSERT_FAILED("TBuffers are not supported");
				break;
			}
			case D3D_SIT_TEXTURE:
			{
				if (res.GetSRVDim() == D3D_SRV_DIMENSION_BUFFER)
					OnNewBufSRV(std::move(res));
				else
					texSRVsRes.emplace_back(std::move(res));
				break;
			}
			case D3D_SIT_SAMPLER:
			{
				OnNewSampler(std::move(res));
				break;
			}
			case D3D_SIT_UAV_RWTYPED:
			{
				if (res.GetSRVDim() == D3D_SRV_DIMENSION_BUFFER)
					OnNewBufUAV(std::move(res));
				else
					OnNewTexUAV(std::move(res));
				break;
			}
			case D3D_SIT_STRUCTURED:
			{
				OnNewBufSRV(std::move(res));
				break;
			}
			case D3D_SIT_UAV_RWSTRUCTURED:
			{
				OnNewBufUAV(std::move(res));
				break;
			}
			case D3D_SIT_BYTEADDRESS:
			{
				OnNewBufSRV(std::move(res));
				break;
			}
			case D3D_SIT_UAV_RWBYTEADDRESS:
			{
				OnNewBufUAV(std::move(res));
				break;
			}
			case D3D_SIT_UAV_APPEND_STRUCTURED:         ASSERT_FAILED("Append structured buffers are not supported"); break;
			case D3D_SIT_UAV_CONSUME_STRUCTURED:        ASSERT_FAILED("Consume structured buffers are not supported"); break;
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: ASSERT_FAILED("RW structured buffers with counter are not supported"); break;
			default:									ASSERT_FAILED("Unexpected resource type");
			}
		}

		for (auto &res : texSRVsRes)
		{
			OnNewTexSRV(std::move(res));
		}
	}
}