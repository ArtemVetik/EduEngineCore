#pragma once
#include "framework.h"
#include "ShaderResourceInfo.h"
#include "ShaderAPI.h"

#include <d3d12shader.h>
#include <Asserts.h>

namespace EduEngine::EduBinding
{
	class EDUBINDING_API ShaderResources
	{
	public:
		ShaderResources(ID3D12ShaderReflection* reflection, const ShaderDesc& desc);
		~ShaderResources();

		uint16 GetCBNum(SHADER_RESOURCE_TYPE t) const { return m_resOffsets[t].TexSRV - m_resOffsets[t].CB; }
		uint16 GetTexSRVNum(SHADER_RESOURCE_TYPE t) const { return m_resOffsets[t].BuffSRV - m_resOffsets[t].TexSRV; }
		uint16 GetBuffSRVNum(SHADER_RESOURCE_TYPE t) const { return m_resOffsets[t].TexUAV - m_resOffsets[t].BuffSRV; }
		uint16 GetTexUAVNum(SHADER_RESOURCE_TYPE t) const { return m_resOffsets[t].BuffUAV - m_resOffsets[t].TexUAV; }
		uint16 GetBuffUAVNum(SHADER_RESOURCE_TYPE t) const { return m_ResEndOffsets[t] - m_resOffsets[t].BuffUAV; }

		ShaderResourceInfo& GetCB(SHADER_RESOURCE_TYPE t, uint16 index) const
		{
			VERIFY_EXPR(index <= GetCBNum(t), "CB index out of range. Index: ", index, ", CB Num: ", GetCBNum(t));
			return m_ResBuffer[m_resOffsets[t].CB + index]; 
		}
		ShaderResourceInfo& GetTexSRV(SHADER_RESOURCE_TYPE t, uint16 index) const
		{
			VERIFY_EXPR(index <= GetTexSRVNum(t), "TexSRV index out of range. Index: ", index, ", TexSRV Num: ", GetTexSRVNum(t));
			return m_ResBuffer[m_resOffsets[t].TexSRV + index];
		}
		ShaderResourceInfo& GetTexUAV(SHADER_RESOURCE_TYPE t, uint16 index) const
		{
			VERIFY_EXPR(index <= GetTexUAVNum(t), "TexUAV index out of range. Index: ", index, ", TexUAV Num: ", GetTexUAVNum(t));
			return m_ResBuffer[m_resOffsets[t].TexUAV + index];
		}
		ShaderResourceInfo& GetBuffSRV(SHADER_RESOURCE_TYPE t, uint16 index) const
		{
			VERIFY_EXPR(index <= GetBuffSRVNum(t), "BuffSRV index out of range. Index: ", index, ", BuffSRV Num: ", GetBuffSRVNum(t));
			return m_ResBuffer[m_resOffsets[t].BuffSRV + index];
		}
		ShaderResourceInfo& GetBuffUAV(SHADER_RESOURCE_TYPE t, uint16 index) const
		{
			VERIFY_EXPR(index <= GetBuffUAVNum(t), "BuffUAV index out of range. Index: ", index, ", BuffUAV Num: ", GetBuffUAVNum(t));
			return m_ResBuffer[m_resOffsets[t].BuffUAV + index];
		}

#ifdef _DEBUG
		void DebugPrint();
#endif

	private:
		struct ResOffsets
		{
			uint32 CB = 0, TexSRV = 0, BuffSRV = 0, TexUAV = 0, BuffUAV = 0;
		};

		ResOffsets m_resOffsets[SHADER_RESOURCE_TYPE_NUM] = {};
		uint16 m_ResEndOffsets[SHADER_RESOURCE_TYPE_NUM] = {};

		ShaderResourceInfo* m_ResBuffer;
	};
}