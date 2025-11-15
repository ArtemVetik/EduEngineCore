#pragma once
#include "framework.h"
#include "ShaderResourceLoader.h"

#include <string>
#include <d3dcompiler.h>

namespace EduEngine::DiligentBinding
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

		ShaderResourceAttribs(ShaderResourceAttribs&& rhs) noexcept :
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

		D3D_SHADER_INPUT_TYPE GetInputType() const { return static_cast<D3D_SHADER_INPUT_TYPE>((PackedAttribs >> InputTypeOffset) & InputTypeMask); }
		SHADER_VARIABLE_TYPE GetVarType() const  { return static_cast<SHADER_VARIABLE_TYPE>((PackedAttribs >> VariableTypeOffset) & VariableTypeMask); }
		D3D_SRV_DIMENSION GetSRVDim() const  { return static_cast<D3D_SRV_DIMENSION>((PackedAttribs >> SRVDimOffset) & SRVDimMask); }
		uint16 GetSamplerId() const { return static_cast<D3D_SRV_DIMENSION>((PackedAttribs >> SamplerIdOffset) & SamplerIdMask); }
		bool HasValidSampler() const { return GetSamplerId() != InvalidSamplerId; }
		bool IsStaticSampler() const { return (PackedAttribs & (1 << StaticSamplerFlagOffset)) != 0; }
		bool IsValidSampler() const { return GetSamplerId() != InvalidSamplerId; }
		bool IsValidBindPoint() const { return BindPoint != InvalidBindPoint; }

	private:
		uint32 PackAttribs(uint16 inputType, uint16 variableType, uint16 srvDim, uint32 samplerId, bool staticSamplerFlag)
		{
			return ((inputType & InputTypeMask) << InputTypeOffset) |
				((variableType & VariableTypeMask) << VariableTypeOffset) |
				((srvDim & SRVDimMask) << SRVDimOffset) |
				((samplerId & SamplerIdMask) << SamplerIdOffset) |
				((staticSamplerFlag ? 1 : 0) << StaticSamplerFlagOffset);
		}

		static constexpr uint32 InputTypeBits = 4;
		static constexpr uint32 InputTypeMask = (1 << InputTypeBits) - 1;
		static constexpr uint32 InputTypeOffset = 0;

		static constexpr uint32 VariableTypeBits = 3;
		static constexpr uint32 VariableTypeMask = (1 << VariableTypeBits) - 1;
		static constexpr uint32 VariableTypeOffset = InputTypeOffset + InputTypeBits;

		static constexpr uint32 SRVDimBits = 4;
		static constexpr uint32 SRVDimMask = (1 << SRVDimBits) - 1;
		static constexpr uint32 SRVDimOffset = VariableTypeOffset + VariableTypeBits;

		static constexpr uint32 SamplerIdBits = 32 - 1 - SRVDimBits - VariableTypeBits - InputTypeBits;
		static constexpr uint32 SamplerIdMask = (1 << SamplerIdBits) - 1;
		static constexpr uint32 SamplerIdOffset = SRVDimOffset + SRVDimBits;

		static constexpr uint32 StaticSamplerFlagBits = 1;
		static constexpr uint32 StaticSamplerFlagMask = (1 << StaticSamplerFlagBits) - 1;
		static constexpr uint32 StaticSamplerFlagOffset = SamplerIdOffset + SamplerIdBits;

		static const uint16 InvalidBindPoint = std::numeric_limits<uint16>::max();
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

		uint32 GetNumCBs()      const noexcept { return (m_TexSRVOffset - 0); }
		uint32 GetNumTexSRV()   const noexcept { return (m_TexUAVOffset - m_TexSRVOffset); }
		uint32 GetNumTexUAV()   const noexcept { return (m_BufSRVOffset - m_TexUAVOffset); }
		uint32 GetNumBufSRV()   const noexcept { return (m_BufUAVOffset - m_BufSRVOffset); }
		uint32 GetNumBufUAV()   const noexcept { return (m_SamplersOffset - m_BufUAVOffset); }
		uint32 GetNumSamplers() const noexcept { return (m_BufferEndOffset - m_SamplersOffset); }

		ShaderResourceAttribs& GetResAttribs(UINT n, UINT offset) const { return m_ResourcesBuffer[offset + n]; }

		ShaderResourceAttribs& GetCB(UINT n) const noexcept { return GetResAttribs(n, 0); }
		ShaderResourceAttribs& GetTexSRV(UINT n) const noexcept { return GetResAttribs(n, m_TexSRVOffset); }
		ShaderResourceAttribs& GetTexUAV(UINT n) const noexcept { return GetResAttribs(n, m_TexUAVOffset); }
		ShaderResourceAttribs& GetBufSRV(UINT n) const noexcept { return GetResAttribs(n, m_BufSRVOffset); }
		ShaderResourceAttribs& GetBufUAV(UINT n) const noexcept { return GetResAttribs(n, m_BufUAVOffset); }
		ShaderResourceAttribs& GetSampler(UINT n) const noexcept { return GetResAttribs(n, m_SamplersOffset); }

		EDU_SHADER_TYPE GetShaderType() const { return m_ShaderType; }

#ifdef _DEBUG
		void DebugPrint();
#endif

		const char* const SamplerSuffix = "_sampler";

		template<typename THandleCB,
				 typename THandleTexSRV,
				 typename THandleTexUAV,
				 typename THandleBufSRV,
				 typename THandleBufUAV>
		void ProcessResources(const SHADER_VARIABLE_TYPE* allowedVarTypes,
							  uint32		numAllowedTypes,
							  THandleCB		HandleCB,
							  THandleTexSRV HandleTexSRV,
							  THandleTexUAV HandleTexUAV,
							  THandleBufSRV HandleBufSRV,
							  THandleBufUAV HandleBufUAV) const
		{
			uint32 allowedTypeBits = 0;

			if (!numAllowedTypes || !allowedVarTypes)
				allowedTypeBits = ~0;

			for (uint32 i = 0; i < numAllowedTypes; i++)
				allowedTypeBits |= (1 << allowedVarTypes[i]);

			auto IsAllowed = [&](SHADER_VARIABLE_TYPE type)
				{
					return (1 << type) & allowedTypeBits;
				};

			for (uint32 n = 0; n < GetNumCBs(); ++n)
			{
				const auto& CB = GetCB(n);
				if (IsAllowed(CB.GetVarType()))
					HandleCB(CB);
			}

			for (uint32 n = 0; n < GetNumTexSRV(); ++n)
			{
				const auto& TexSRV = GetTexSRV(n);
				if (IsAllowed(TexSRV.GetVarType()))
					HandleTexSRV(TexSRV);
			}

			for (uint32 n = 0; n < GetNumTexUAV(); ++n)
			{
				const auto& TexUAV = GetTexUAV(n);
				if (IsAllowed(TexUAV.GetVarType()))
					HandleTexUAV(TexUAV);
			}

			for (uint32 n = 0; n < GetNumBufSRV(); ++n)
			{
				const auto& BufSRV = GetBufSRV(n);
				if (IsAllowed(BufSRV.GetVarType()))
					HandleBufSRV(BufSRV);
			}

			for (uint32 n = 0; n < GetNumBufUAV(); ++n)
			{
				const auto& BufUAV = GetBufUAV(n);
				if (IsAllowed(BufUAV.GetVarType()))
					HandleBufUAV(BufUAV);
			}
		}

	private:
		EDU_SHADER_TYPE m_ShaderType;

		ShaderResourceAttribs* m_ResourcesBuffer;

		uint16 m_TexSRVOffset = 0;
		uint16 m_TexUAVOffset = 0;
		uint16 m_BufSRVOffset = 0;
		uint16 m_BufUAVOffset = 0;
		uint16 m_SamplersOffset = 0;
		uint16 m_BufferEndOffset = 0;
	};
}