#pragma once
#include "ShaderAPI.h"

#include <d3d12shader.h>

namespace EduEngine::EduBinding
{
	struct EDUBINDING_API ShaderResourceInfo
	{
	public:
		ShaderResourceInfo(const char* name,
						   D3D_SHADER_INPUT_TYPE inputType,
						   SHADER_RESOURCE_TYPE resType,
						   uint16 bindPoint,
						   uint16 bindCount,
						   uint16 space,
						   D3D_SRV_DIMENSION dimension) :
			m_Name(name),
			m_InputType(inputType),
			m_BindPoint(bindPoint),
			m_ResType(resType),
			m_BindCount(bindCount),
			m_Space(space),
			m_Dimension(dimension)
		{ }

		ShaderResourceInfo(const ShaderResourceInfo&) = delete;
		ShaderResourceInfo& operator = (const ShaderResourceInfo&) = delete;

		ShaderResourceInfo(ShaderResourceInfo&&) = delete;
		ShaderResourceInfo& operator = (ShaderResourceInfo&&) = delete;

		const char* GetName() const { return m_Name; }
		D3D_SHADER_INPUT_TYPE GetInputType() const { return m_InputType; }
		SHADER_RESOURCE_TYPE GetResType() const { return m_ResType; }
		uint16 GetBindPoint() const { return m_BindPoint; }
		uint16 GetBindCount() const { return m_BindCount; }
		uint16 GetSpace() const { return m_Space; }
		D3D_SRV_DIMENSION GetSRVDim() const { return m_Dimension; }

	private:
		const char* m_Name;
		D3D_SHADER_INPUT_TYPE m_InputType;
		SHADER_RESOURCE_TYPE m_ResType;
		uint16 m_BindPoint;
		uint16 m_BindCount;
		uint16 m_Space;
		D3D_SRV_DIMENSION m_Dimension;
	};
}