#include "ShaderResourcesD3D12.h"
#include "DebugEnumPrint.h"

namespace EduEngine
{
	ShaderResourcesD3D12::ShaderResourcesD3D12(ID3D12ShaderReflection* reflection, const ShaderDesc& shaderDesc) :
		m_ShaderType(shaderDesc.ShaderType),
		m_ResourcesBuffer{nullptr}
	{
		UINT currCB = 0, currTexSRV = 0, currTexUAV = 0, currBufSRV = 0, currBufUAV = 0, currSampler = 0;

		LoadD3D12ShaderResource(
			reflection,

			// OnResourceCounted
			[&](UINT numCBs, UINT numTexSRVs, UINT numTexUAVs, UINT numBufSRVs, UINT numBufUAVs, UINT numSamplers)
			{
				VERIFY_EXPR(numCBs < UINT16_MAX, "numCBs out of bounds");
				m_TexSRVOffset = 0 + static_cast<uint16>(numCBs);
				VERIFY_EXPR(numTexSRVs < UINT16_MAX, "numTexSRVs out of bounds");
				m_TexUAVOffset = m_TexSRVOffset + static_cast<uint16>(numTexSRVs);
				VERIFY_EXPR(numTexUAVs < UINT16_MAX, "numTexUAVs out of bounds");
				m_BufSRVOffset = m_TexUAVOffset + static_cast<uint16>(numTexUAVs);
				VERIFY_EXPR(numBufSRVs < UINT16_MAX, "numBufSRVs out of bounds");
				m_BufUAVOffset = m_BufSRVOffset + static_cast<uint16>(numBufSRVs);
				VERIFY_EXPR(numBufUAVs < UINT16_MAX, "numBufUAVs out of bounds");
				m_SamplersOffset = m_BufUAVOffset + static_cast<uint16>(numBufUAVs);
				VERIFY_EXPR(numSamplers < UINT16_MAX, "numSamplers out of bounds");
				m_BufferEndOffset = m_SamplersOffset + static_cast<uint16>(numSamplers);

				if (m_BufferEndOffset)
					m_ResourcesBuffer = static_cast<ShaderResourceAttribs*>(std::malloc(sizeof(ShaderResourceAttribs) * m_BufferEndOffset));
			},

			// OnNewCB
			[&](ShaderResourceAttribs& attribs)
			{
				new (&GetCB(currCB++)) ShaderResourceAttribs(std::move(attribs));
			},

			// OnNewTexUAV
			[&](ShaderResourceAttribs& attribs)
			{
				new (&GetTexUAV(currTexUAV++)) ShaderResourceAttribs(std::move(attribs));
			},

			// OnNewBuffUAV
			[&](ShaderResourceAttribs& attribs)
			{
				new (&GetBufUAV(currBufUAV++)) ShaderResourceAttribs(std::move(attribs));
			},

			// OnNewBuffSRV
			[&](ShaderResourceAttribs& attribs)
			{
				new (&GetBufSRV(currBufSRV++)) ShaderResourceAttribs(std::move(attribs));
			},

			// OnNewSampler
			[&](ShaderResourceAttribs& attribs)
			{
				VERIFY_EXPR(StrHasSuff(attribs.Name, SamplerSuffix), "Sampler must have suffix \"_sampler\"");
				new (&GetSampler(currSampler++)) ShaderResourceAttribs(std::move(attribs));
			},

			// OnNewTexSRV
			[&](ShaderResourceAttribs& attribs)
			{
				VERIFY_EXPR(currSampler == GetNumSamplers(), "All samplers must be initialized before texture SRVs");

				auto numSamplers = GetNumSamplers();
				uint32 samplerId = ShaderResourceAttribs::InvalidSamplerId;

				for (uint16 i = 0; i < numSamplers; i++)
				{
					auto& sampler = GetSampler(i);
					if (StrCmpSuff(sampler.Name, attribs.Name, SamplerSuffix))
					{
						samplerId = static_cast<uint32>(i);
						break;
					}
				}

				new (&GetTexSRV(currTexSRV++)) ShaderResourceAttribs(std::move(attribs), samplerId);
			},

			shaderDesc
		);
	}

	ShaderResourcesD3D12::~ShaderResourcesD3D12()
	{
		for (uint32 i = 0; i < m_BufferEndOffset; i++)
			m_ResourcesBuffer[i].~ShaderResourceAttribs();

		std::free(m_ResourcesBuffer);
	}

#ifdef _DEBUG
	void ShaderResourcesD3D12::DebugPrint()
	{
		printf("-------------------------------------------------------------\n");
		printf("----------------- [%s] ShaderResources -----------------\n", ShaderTypeStr(m_ShaderType));
		printf("-------------------------------------------------------------\n");
		ProcessResources(
			nullptr, 0,
			[&](const ShaderResourceAttribs& a)
			{
				printf("[CB] Name: %s\tBindPoint: %d\tBindCount: %d\tVarType: %s\tInputType: %s\tSrvDim: %s\tSamplerId: %d\n",
					a.Name, a.BindPoint, a.BindCount, VarTypeStr(a.GetVarType()), InputTypeStr(a.GetInputType()), SrvDimStr(a.GetSRVDim()), a.GetSamplerId());
			},
			[&](const ShaderResourceAttribs& a)
			{
				printf("[TexSRV] Name: %s\tBindPoint: %d\tBindCount: %d\tVarType: %s\tInputType: %s\tSrvDim: %s\tSamplerId: %d\n",
					a.Name, a.BindPoint, a.BindCount, VarTypeStr(a.GetVarType()), InputTypeStr(a.GetInputType()), SrvDimStr(a.GetSRVDim()), a.GetSamplerId());

				if (a.HasValidSampler())
				{
					auto samplerId = a.GetSamplerId();
					const auto& samplerAttribs = GetSampler(samplerId);
					
					printf("[Sampler %s] Name: %s\tBindPoint: %d\tBindCount: %d\tVarType: %s\tInputType: %s\tSrvDim: %s\tSamplerId: %d\n",
						samplerAttribs.IsStaticSampler() ? "Static" : "Mutable",
						samplerAttribs.Name, samplerAttribs.BindPoint, samplerAttribs.BindCount, VarTypeStr(samplerAttribs.GetVarType()), InputTypeStr(samplerAttribs.GetInputType()), SrvDimStr(samplerAttribs.GetSRVDim()), samplerAttribs.GetSamplerId());
				}
			},
			[&](const ShaderResourceAttribs& a)
			{
				printf("[TexUAV] Name: %s\tBindPoint: %d\tBindCount: %d\tVarType: %s\tInputType: %s\tSrvDim: %s\tSamplerId: %d\n",
					a.Name, a.BindPoint, a.BindCount, VarTypeStr(a.GetVarType()), InputTypeStr(a.GetInputType()), SrvDimStr(a.GetSRVDim()), a.GetSamplerId());
			},
			[&](const ShaderResourceAttribs& a)
			{
				printf("[BuffSRV] Name: %s\tBindPoint: %d\tBindCount: %d\tVarType: %s\tInputType: %s\tSrvDim: %s\tSamplerId: %d\n",
					a.Name, a.BindPoint, a.BindCount, VarTypeStr(a.GetVarType()), InputTypeStr(a.GetInputType()), SrvDimStr(a.GetSRVDim()), a.GetSamplerId());
			},
			[&](const ShaderResourceAttribs& a)
			{
				printf("[BuffUAV] Name: %s\tBindPoint: %d\tBindCount: %d\tVarType: %d\tInputType: %s\tSrvDim: %s\tSamplerId: %d\n",
					a.Name, a.BindPoint, a.BindCount, VarTypeStr(a.GetVarType()), InputTypeStr(a.GetInputType()), SrvDimStr(a.GetSRVDim()), a.GetSamplerId());
			}
		);
	}
#endif
}