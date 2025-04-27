#pragma once

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
	{ }
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