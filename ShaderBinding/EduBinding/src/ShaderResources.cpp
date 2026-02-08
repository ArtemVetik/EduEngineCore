#include "ShaderResources.h"
#include "DebugEnumPrint.h"

namespace EduEngine::EduBinding
{
	ShaderResources::ShaderResources(ID3D12ShaderReflection* reflection, const ShaderDesc& desc)
	{
		D3D12_SHADER_DESC shaderDesc;
		reflection->GetDesc(&shaderDesc);

		ShaderResourceInfo* resources = (ShaderResourceInfo*)malloc(shaderDesc.BoundResources * sizeof(ShaderResourceInfo));

		ResOffsets resNums[SHADER_RESOURCE_TYPE_NUM] = {};

		for (int i = 0; i < shaderDesc.BoundResources; i++)
		{
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			reflection->GetResourceBindingDesc(i, &bindDesc);

			SHADER_RESOURCE_TYPE resType = desc.DefaultType;

			for (int i = 0; i < desc.ResourceNum; i++)
			{
				if (strcmp(desc.ResourceDesc[i].Name, bindDesc.Name) == 0)
				{
					resType = desc.ResourceDesc[i].Type;
					break;
				}
			}

			ResOffsets* currNums = &resNums[resType];

			switch (bindDesc.Type)
			{
			case D3D_SIT_CBUFFER:                       ++currNums->CB; break;
			case D3D_SIT_TBUFFER:                       ASSERT_FAILED("TBuffers are not supported"); break;
			case D3D_SIT_TEXTURE:                       ++(bindDesc.Dimension == D3D_SRV_DIMENSION_BUFFER ? currNums->BuffSRV : currNums->TexSRV); break;
			case D3D_SIT_SAMPLER:						break;//LOG_ERROR("Samplers are not supported"); break; // TODO: support samplers
			case D3D_SIT_UAV_RWTYPED:                   ++(bindDesc.Dimension == D3D_SRV_DIMENSION_BUFFER ? currNums->BuffUAV : currNums->TexUAV); break;
			case D3D_SIT_STRUCTURED:                    ++currNums->BuffSRV; break;
			case D3D_SIT_UAV_RWSTRUCTURED:              ++currNums->BuffUAV; break;
			case D3D_SIT_BYTEADDRESS:                   ++currNums->BuffSRV; break;
			case D3D_SIT_UAV_RWBYTEADDRESS:             ++currNums->BuffUAV; break;
			case D3D_SIT_UAV_APPEND_STRUCTURED:         ASSERT_FAILED("Append structured buffers are not supported"); break;
			case D3D_SIT_UAV_CONSUME_STRUCTURED:        ASSERT_FAILED("Consume structured buffers are not supported"); break;
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: ASSERT_FAILED("RW structured buffers with counter are not supported"); break;
			default:									ASSERT_FAILED("Unexpected resource type");
			}

			new (&resources[i]) ShaderResourceInfo(bindDesc.Name, bindDesc.Type, resType, bindDesc.BindPoint, bindDesc.BindCount, bindDesc.Space, bindDesc.Dimension);
		}

		for (size_t i = 0; i < SHADER_RESOURCE_TYPE_NUM; i++)
		{
			if (i > 0)
				m_resOffsets[i].CB = m_resOffsets[i - 1].BuffUAV + resNums[i - 1].BuffUAV;

			m_resOffsets[i].TexSRV = m_resOffsets[i].CB + resNums[i].CB;
			m_resOffsets[i].BuffSRV += m_resOffsets[i].TexSRV + resNums[i].TexSRV;
			m_resOffsets[i].TexUAV = m_resOffsets[i].BuffSRV + resNums[i].BuffSRV;
			m_resOffsets[i].BuffUAV += m_resOffsets[i].TexUAV + resNums[i].TexUAV;

			m_ResEndOffsets[i] = m_resOffsets[i].BuffUAV + resNums[i].BuffUAV;
		}

		m_ResBuffer = (ShaderResourceInfo*)malloc(sizeof(ShaderResourceInfo) * shaderDesc.BoundResources);

		auto AddRes = [](void* addr, const ShaderResourceInfo& res)
			{
				new (addr) ShaderResourceInfo(res.GetName(), res.GetInputType(), res.GetResType(), res.GetBindPoint(), res.GetBindCount(), res.GetSpace(), res.GetSRVDim());
			};

		memset(&resNums, 0, sizeof(ResOffsets) * SHADER_RESOURCE_TYPE_NUM);

		for (int i = 0; i < shaderDesc.BoundResources; i++)
		{
			auto& res = resources[i];

			switch (res.GetInputType())
			{
			case D3D_SIT_CBUFFER:
				AddRes(&GetCB(res.GetResType(), resNums[res.GetResType()].CB++), res);
				break;
			case D3D_SIT_TEXTURE:
				if (res.GetSRVDim() == D3D_SRV_DIMENSION_BUFFER)
					AddRes(&GetBuffSRV(res.GetResType(), resNums[res.GetResType()].BuffSRV++), res);
				else
					AddRes(&GetTexSRV(res.GetResType(), resNums[res.GetResType()].TexSRV++), res);
				break;
			case D3D_SIT_UAV_RWTYPED:
				if (res.GetSRVDim() == D3D_SRV_DIMENSION_BUFFER)
					AddRes(&GetBuffUAV(res.GetResType(), resNums[res.GetResType()].BuffUAV++), res);
				else
					AddRes(&GetTexUAV(res.GetResType(), resNums[res.GetResType()].TexUAV++), res);
				break;
			case D3D_SIT_STRUCTURED:
			case D3D_SIT_BYTEADDRESS:
				AddRes(&GetBuffSRV(res.GetResType(), resNums[res.GetResType()].BuffSRV++), res);
				break;
			case D3D_SIT_UAV_RWSTRUCTURED:
			case D3D_SIT_UAV_RWBYTEADDRESS:
				AddRes(&GetBuffUAV(res.GetResType(), resNums[res.GetResType()].BuffUAV++), res);
				break;
			case D3D_SIT_SAMPLER:
				break;
			default:
				ASSERT_FAILED("Unexpected resource type");
			}
		}

		free(resources);
	}

	ShaderResources::~ShaderResources()
	{
		free(m_ResBuffer);
	}

#ifdef EDUBINDINGDEBUG
	void ShaderResources::DebugPrint()
	{
		auto PrintShaderRes = [](const ShaderResourceInfo& res)
			{
				printf("\tName: %s\n\t\tInputType: %s\tBindPoint: %d\tBindCount: %d\tSpace: %d\tDim: %s\n",
					res.GetName(), InputTypeStr(res.GetInputType()), res.GetBindPoint(), res.GetBindCount(), res.GetSpace(), SrvDimStr(res.GetSRVDim()));
			};

		for (uint16 rt = 0; rt < SHADER_RESOURCE_TYPE_NUM; rt++)
		{
			SHADER_RESOURCE_TYPE t = (SHADER_RESOURCE_TYPE)(rt);

			printf("%s\n", ShaderResourceTypeStr(t));

			for (uint16 i = 0; i < GetCBNum(t); i++)
			{
				auto& cb = GetCB(t, i);
				VERIFY_EXPR(cb.GetResType() == t, "Resource type mismatch");
				PrintShaderRes(cb);
			}

			for (uint16 i = 0; i < GetTexSRVNum(t); i++)
			{
				auto& texSRV = GetTexSRV(t, i);
				VERIFY_EXPR(texSRV.GetResType() == t, "Resource type mismatch");
				PrintShaderRes(texSRV);
			}

			for (uint16 i = 0; i < GetBuffSRVNum(t); i++)
			{
				auto& buffSRV = GetBuffSRV(t, i);
				VERIFY_EXPR(buffSRV.GetResType() == t, "Resource type mismatch");
				PrintShaderRes(buffSRV);
			}

			for (uint16 i = 0; i < GetTexUAVNum(t); i++)
			{
				auto& texUAV = GetTexUAV(t, i);
				VERIFY_EXPR(texUAV.GetResType() == t, "Resource type mismatch");
				PrintShaderRes(texUAV);
			}

			for (uint16 i = 0; i < GetBuffUAVNum(t); i++)
			{
				auto& buffUAV = GetBuffUAV(t, i);
				VERIFY_EXPR(buffUAV.GetResType() == t, "Resource type mismatch");
				PrintShaderRes(buffUAV);
			}
		}

		printf("\n\n");
	}
#endif
}