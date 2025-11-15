#pragma once
#include "framework.h"
#include "ShaderResources.h"

#pragma comment(lib, "dxcompiler.lib")

#include <memory>
#include <atlbase.h>
#include <dxcapi.h>
#include <d3d12shader.h>

namespace EduEngine::EduBinding
{
	class EDUBINDING_API ShaderD3D12
	{
	public:
		ShaderD3D12(const wchar_t*	  name,
					const wchar_t*	  entryPoint,
					const wchar_t*	  target,
					const LPCWSTR*	  defines,
					const ShaderDesc& desc);

		EDU_SHADER_TYPE GetType() const { return m_Type; };
		ShaderResources* GetResources() const { return m_Resources.get(); }

		D3D12_SHADER_BYTECODE GetShaderBytecode() const;

#ifdef _DEBUG
		void DebugPrint();
#endif

	private:
		CComPtr<IDxcBlob> m_ShaderBlob;
		CComPtr<ID3D12ShaderReflection> m_Reflection;
		std::shared_ptr<ShaderResources> m_Resources;
		EDU_SHADER_TYPE m_Type;

		const wchar_t* m_Name; // TODO: needs only in debug
	};
}