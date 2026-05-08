#pragma once
#include <EngineTypes.h>

namespace EduEngine::EduBinding
{
	enum EDU_SHADER_TYPE
	{
		EDU_SHADER_TYPE_VERTEX = 0,
		EDU_SHADER_TYPE_HULL = 1,
		EDU_SHADER_TYPE_DOMAIN = 2,
		EDU_SHADER_TYPE_GEOMETRY = 3,
		EDU_SHADER_TYPE_PIXEL = 4,
		EDU_SHADER_TYPE_COMPUTE = 5,
		EDU_SHADER_TYPE_AMPLIFICATION = 6,
		EDU_SHADER_TYPE_MESH = 7,
		EDU_SHADER_TYPE_NUM = 8,
	};

	enum SHADER_RESOURCE_TYPE
	{
		SHADER_RESOURCE_TYPE_DYNAMIC = 0,
		SHADER_RESOURCE_TYPE_MUTABLE = 1,
		SHADER_RESOURCE_TYPE_NUM = 2,
	};

	struct ShaderResourceDesc
	{
		ShaderResourceDesc(const char* name, SHADER_RESOURCE_TYPE type) :
			Name(name),
			Type(type)
		{
		}

		const char* Name;
		SHADER_RESOURCE_TYPE Type;
	};

	class ShaderDesc
	{
	public:
		ShaderDesc()
		{
			ResourceNum = 0;
			ResourceDesc = nullptr;
			DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		}

		ShaderDesc(ShaderDesc& rhs) = delete;
		ShaderDesc& operator = (ShaderDesc& rhs) = delete;

		uint16 ResourceNum;
		ShaderResourceDesc* ResourceDesc;

		SHADER_RESOURCE_TYPE DefaultType;
	};
}