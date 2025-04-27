#pragma once
#include "framework.h"
#include "ShaderResourceLoader.h"

#include <string>
#include <d3dcompiler.h>

namespace EduEngine
{
	struct ShaderResourceAttribs
	{
	public:
		const char* const Name;
		const uint16 BindPoint;
		const uint16 BindCount;

		//            4              3               4              20                1
		// bit | 0  1  2  3   |  4   5   6   |  7  8  9  10 | 11  12 ...  30 |        31         |   
		//     |              |              |              |                |                   |
		//     |  InputType   | VariableType |   SRV Dim    |    SamplerId   | StaticSamplerFlag |
		const uint32 PackedAttribs;

		static constexpr uint32 InvalidSamplerId = static_cast<uint32>(-1);

		ShaderResourceAttribs(const char*			name,
							  uint16				bindPoint,
							  uint16				bindCount,
							  D3D_SHADER_INPUT_TYPE inputType,
							  SHADER_VARIABLE_TYPE	variableType,
							  D3D_SRV_DIMENSION		srvDimension,
							  uint32				samplerId,
							  bool					staticSampleFlag) :
			Name{ name },
			BindPoint{ bindPoint },
			BindCount{ bindCount },
			PackedAttribs{ PackAttribs(inputType, variableType, srvDimension, samplerId, staticSampleFlag) }
		{
		};

		ShaderResourceAttribs(ShaderResourceAttribs&& rhs) :
			Name(rhs.Name),
			BindPoint(rhs.BindPoint),
			BindCount(rhs.BindCount),
			PackedAttribs(rhs.PackedAttribs)
		{
		};

		ShaderResourceAttribs(ShaderResourceAttribs&& rhs, uint32 samplerId) :
			Name(rhs.Name),
			BindPoint(rhs.BindPoint),
			BindCount(rhs.BindCount),
			PackedAttribs(PackAttribs(rhs.GetInputType(), rhs.GetVarType(), rhs.GetSRVDim(), samplerId, false))
		{
		};

		ShaderResourceAttribs(const ShaderResourceAttribs&) = delete;
		ShaderResourceAttribs& operator = (const ShaderResourceAttribs&) = delete;
		ShaderResourceAttribs& operator = (ShaderResourceAttribs&&) = delete;

		D3D_SHADER_INPUT_TYPE GetInputType() { return static_cast<D3D_SHADER_INPUT_TYPE>((PackedAttribs >> InputTypeOffset) & InputTypeMask); }
		SHADER_VARIABLE_TYPE GetVarType() { return static_cast<SHADER_VARIABLE_TYPE>((PackedAttribs >> VariableTypeOffset) & VariableTypeMask); }
		D3D_SRV_DIMENSION GetSRVDim() { return static_cast<D3D_SRV_DIMENSION>((PackedAttribs >> SRVDimOffset) & SRVDimMask); }
		uint16 GetSamplerId() { return static_cast<D3D_SRV_DIMENSION>((PackedAttribs >> SRVDimOffset) & SRVDimMask); }

	private:
		uint32 PackAttribs(uint16 inputType, uint16 variableType, uint16 srvDim, uint32 samplerId, bool staticSamplerFlag)
		{
			return inputType |
				(variableType << VariableTypeOffset) |
				(srvDim << SRVDimOffset) |
				(samplerId << SamplerIdOffset) |
				((staticSamplerFlag ? 1 : 0) << StaticSamplerFlagOffset);
		}

		static constexpr uint16 InputTypeBits = 4;
		static constexpr uint16 InputTypeMask = (1 << InputTypeBits) - 1;
		static constexpr uint16 InputTypeOffset = 0;

		static constexpr uint16 VariableTypeBits = 3;
		static constexpr uint16 VariableTypeMask = (1 << VariableTypeBits) - 1;
		static constexpr uint16 VariableTypeOffset = InputTypeOffset + InputTypeBits;

		static constexpr uint16 SRVDimBits = 4;
		static constexpr uint16 SRVDimMask = (1 << SRVDimBits) - 1;
		static constexpr uint16 SRVDimOffset = VariableTypeOffset + SRVDimBits;
		
		static constexpr uint16 SamplerIdBits = 32 - 1 - SRVDimBits - VariableTypeBits - InputTypeBits;
		static constexpr uint16 SamplerIdMask = (1 << SamplerIdBits) - 1;
		static constexpr uint16 SamplerIdOffset = SRVDimOffset + SamplerIdBits;

		static constexpr uint16 StaticSamplerFlagBits = 1;
		static constexpr uint16 StaticSamplerFlagMask = (1 << StaticSamplerFlagBits) - 1;
		static constexpr uint16 StaticSamplerFlagOffset = SamplerIdOffset + StaticSamplerFlagBits;
	};

	class ShaderResourcesD3D12
	{
	public:
		ShaderResourcesD3D12(ID3D12ShaderReflection* reflection, const ShaderDesc& shaderDesc);
		~ShaderResourcesD3D12();

		ShaderResourcesD3D12(const ShaderResourcesD3D12&) = delete;
		ShaderResourcesD3D12(ShaderResourcesD3D12&&) = delete;
		ShaderResourcesD3D12& operator = (const ShaderResourcesD3D12&) = delete;
		ShaderResourcesD3D12& operator = (ShaderResourcesD3D12&&) = delete;

	private:

		ShaderResourceAttribs& GetResAttribs(UINT n, UINT offset) { return m_ResourcesBuffer[offset + n]; }

		ShaderResourceAttribs& GetCB(UINT n) { return GetResAttribs(n, 0); }
		ShaderResourceAttribs& GetTexSRV(UINT n) { return GetResAttribs(n, m_TexSRVOffset); }
		ShaderResourceAttribs& GetTexUAV(UINT n) { return GetResAttribs(n, m_TexUAVOffset); }
		ShaderResourceAttribs& GetBufSRV(UINT n) { return GetResAttribs(n, m_BufSRVOffset); }
		ShaderResourceAttribs& GetBufUAV(UINT n) { return GetResAttribs(n, m_BufUAVOffset); }
		ShaderResourceAttribs& GetSampler(UINT n) { return GetResAttribs(n, m_SamplersOffset); }

		uint16 GetNumSamplers() const { return m_BufferEndOffset - m_SamplersOffset; }

		const char* const SamplerSuffix = "_sampler";

	private:
		ShaderResourceAttribs* m_ResourcesBuffer;

		uint16 m_TexSRVOffset = 0;
		uint16 m_TexUAVOffset = 0;
		uint16 m_BufSRVOffset = 0;
		uint16 m_BufUAVOffset = 0;
		uint16 m_SamplersOffset = 0;
		uint16 m_BufferEndOffset = 0;
	};
}