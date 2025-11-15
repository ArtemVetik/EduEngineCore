#pragma once
#include "framework.h"
#include "ShaderAPI.h"

#include <ResourceViewD3D12.h>
#include <DynamicUploadBuffer.h>

namespace EduEngine::EduBinding
{
	class EDUBINDING_API IShaderBinder
	{
	public:
		virtual void BindResource(EDU_SHADER_TYPE shaderType, const char* name, std::shared_ptr<ResourceViewD3D12> resource) = 0;
		virtual void BindDynamicResource(EDU_SHADER_TYPE shaderType, const char* name, std::shared_ptr<DynamicUploadBuffer> resource) = 0;
	};
}