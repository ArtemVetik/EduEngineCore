#pragma once
#include "framework.h"
#include "ShaderResourceLoader.h"

#include <string>
#include <d3dcompiler.h>

namespace EduEngine
{
	enum SHADER_VARIABLE_TYPE
	{
		STATIC = 0,
		MUTABLE = 1,
		DYNAMIC = 2
	};

	struct ShaderVariableDesc
	{
		const char* Name;
		SHADER_VARIABLE_TYPE Type;

		ShaderVariableDesc(const char* name, SHADER_VARIABLE_TYPE type) :
			Name(name), Type(type)
		{
		}
	};

	struct ShaderDesc
	{
		SHADER_VARIABLE_TYPE DefaultVarType;
		ShaderVariableDesc* VarDesc;
		UINT numVarDesc;
	};

	inline SHADER_VARIABLE_TYPE GetShaderVariableType(const char* name, const ShaderDesc& shaderDesc)
	{
		for (UINT i = 0; i < shaderDesc.numVarDesc; i++)
		{
			if (strcmp(name, shaderDesc.VarDesc[i].Name) == 0)
				return shaderDesc.VarDesc[i].Type;
		}

		return shaderDesc.DefaultVarType;
	}

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

		ShaderResourceAttribs(const ShaderResourceAttribs&) = delete;
		ShaderResourceAttribs& operator = (const ShaderResourceAttribs&) = delete;
		ShaderResourceAttribs& operator = (ShaderResourceAttribs&&) = delete;

		D3D_SHADER_INPUT_TYPE GetInputType() { return static_cast<D3D_SHADER_INPUT_TYPE>((PackedAttribs >> InputTypeOffset) & InputTypeMask); }
		D3D_SRV_DIMENSION GetSRVDim() { return static_cast<D3D_SRV_DIMENSION>((PackedAttribs >> SRVDimOffset) & SRVDimMask); }

	private:
		uint32 PackAttribs(uint16 inputType, uint16 variableType, uint16 srvDim, uint16 samplerId, bool staticSamplerFlag)
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