#pragma once
#include "framework.h"
#include "ShaderResourcesD3D12.h"
#include "ShaderResourceLayoutD3D12.h"
#include "ShaderAPI.h"

#include <wrl.h>
#include <dxcapi.h>

#pragma comment(lib,"dxcompiler.lib")

namespace EduEngine::DiligentBinding
{
	inline D3D12_SHADER_VISIBILITY GetShaderVisibility(EDU_SHADER_TYPE type)
	{
		switch (type)
		{
		case EDU_SHADER_TYPE_VERTEX: return D3D12_SHADER_VISIBILITY_VERTEX;
		case EDU_SHADER_TYPE_GEOMETRY: return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case EDU_SHADER_TYPE_PIXEL: return D3D12_SHADER_VISIBILITY_PIXEL;
		case EDU_SHADER_TYPE_COMPUTE: return D3D12_SHADER_VISIBILITY_ALL;
		default: ASSERT_FAILED("Unknown shader type");
		}
	}

	class DILIGENTBINDING_API ShaderD3D12
	{
	public:
		ShaderD3D12(std::wstring	   fileName,
					const LPCWSTR*	   defines,
					std::wstring	   entryPoint,
					std::wstring	   target,
					RenderDeviceD3D12* device,
					const ShaderDesc&  desc);

		ShaderResourceLayoutD3D12::SRV_CBV_UAV& GetStaticVariable(const char* name) { return m_StaticLayout->GetVariable(name); }

		D3D12_SHADER_BYTECODE GetShaderBytecode() const;
		Microsoft::WRL::ComPtr<IDxcBlob> GetShaderBlob() const { return m_ShaderBlob; }
		ShaderDesc GetDesc() const { return m_Desc; }
		EDU_SHADER_TYPE GetShaderType() const { return m_Desc.ShaderType; }

		std::shared_ptr<ShaderResourcesD3D12>& GetShaderResources() { return m_ShaderResources; }
		ShaderResourceLayoutD3D12* GetStaticLayout() { return m_StaticLayout.get(); }

#ifdef _DEBUG
		void DebugPrint();
#endif

	private:
		Microsoft::WRL::ComPtr<IDxcBlob> m_ShaderBlob;
		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> m_ShaderReflection;

		ShaderDesc m_Desc;

		std::shared_ptr<ShaderResourcesD3D12> m_ShaderResources;
		std::shared_ptr<ShaderResourceLayoutD3D12> m_StaticLayout;
		std::shared_ptr<ShaderResourceCacheD3D12> m_StaticCache;

#ifdef _DEBUG
		std::wstring m_FileName;
		std::wstring m_EntryPoint;
#endif
	};
}

