#pragma once

namespace EduEngine
{
	enum GRAPHICS_API SHADER_VARIABLE_TYPE
	{
		SHADER_VARIABLE_TYPE_STATIC = 0,
		SHADER_VARIABLE_TYPE_MUTABLE = 1,
		SHADER_VARIABLE_TYPE_DYNAMIC = 2,
		SHADER_VARIABLE_TYPE_NUM_TYPES = 3,
	};

	enum EDU_SHADER_TYPE
	{
		EDU_SHADER_TYPE_VERTEX = 0,
		EDU_SHADER_TYPE_GEOMETRY = 1,
		EDU_SHADER_TYPE_PIXEL = 2,
		EDU_SHADER_TYPE_COMPUTE = 3,
		EDU_SHADER_TYPE_NUM_TYPES = 4,
	};

	struct GRAPHICS_API ShaderVariableDesc
	{
		const char* Name;
		SHADER_VARIABLE_TYPE Type;

		ShaderVariableDesc(const char* name, SHADER_VARIABLE_TYPE type) :
			Name(name), Type(type)
		{
		}
	};

	struct StaticSamplerDesc
	{
		D3D12_STATIC_SAMPLER_DESC Desc;
		const char* TextureName = nullptr;
	};

	struct GRAPHICS_API ShaderDesc
	{
		EDU_SHADER_TYPE ShaderType;
		SHADER_VARIABLE_TYPE DefaultVarType;
		ShaderVariableDesc* VarDesc;
		uint32 NumVarDesc;
		StaticSamplerDesc* StaticSamplers;
		uint32 NumStaticSamplers;

		ShaderDesc() :
			ShaderType(EDU_SHADER_TYPE_VERTEX),
			DefaultVarType(SHADER_VARIABLE_TYPE_MUTABLE),
			NumVarDesc(0),
			VarDesc(nullptr),
			NumStaticSamplers(0),
			StaticSamplers(nullptr)
		{ }
	};

	inline SHADER_VARIABLE_TYPE GetShaderVariableType(const char* name, const ShaderDesc& shaderDesc)
	{
		for (UINT i = 0; i < shaderDesc.NumVarDesc; i++)
		{
			if (strcmp(name, shaderDesc.VarDesc[i].Name) == 0)
				return shaderDesc.VarDesc[i].Type;
		}

		return shaderDesc.DefaultVarType;
	}
}