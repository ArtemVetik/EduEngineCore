#include "ShaderResourcesD3D12.h"

namespace EduEngine
{
	ShaderResourcesD3D12::ShaderResourcesD3D12(ID3D12ShaderReflection* reflection, const ShaderDesc& shaderDesc)
	{
		UINT currCB = 0, currTexSRV = 0, currTexUAV = 0, currBufSRV = 0, currBufUAV = 0, currSampler = 0;

		LoadD3D12ShaderResource(
			reflection,

			// OnResourceCounted
			[&](UINT numCBs, UINT numTexSRVs, UINT numTexUAVs, UINT numBufSRVs, UINT numBufUAVs, UINT numSamplers)
			{
				// TODO: add limit asserts
				m_TexSRVOffset = 0 + static_cast<uint16>(numCBs);
				m_TexUAVOffset = m_TexSRVOffset + static_cast<uint16>(numTexSRVs);
				m_BufSRVOffset = m_TexUAVOffset + static_cast<uint16>(numTexUAVs);
				m_BufUAVOffset = m_BufSRVOffset + static_cast<uint16>(numBufSRVs);
				m_SamplersOffset = m_BufUAVOffset + static_cast<uint16>(numBufUAVs);
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
				new (&GetSampler(currSampler++)) ShaderResourceAttribs(std::move(attribs));
			},

			// OnNewTexSRV
			[&](ShaderResourceAttribs& attribs)
			{
				new (&GetTexSRV(currTexSRV++)) ShaderResourceAttribs(std::move(attribs));
			},

			shaderDesc
		);
	}

	ShaderResourcesD3D12::~ShaderResourcesD3D12()
	{
		std::free(m_ResourcesBuffer);
	}
}