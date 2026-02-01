#include "ShaderD3D12.h"
#include "DebugEnumPrint.h"

#include <StringUtils.h>
#include <Asserts.h>
#include <vector>

namespace EduEngine::EduBinding
{
	__forceinline EDU_SHADER_TYPE GetTypeFromTarget(const wchar_t* target)
	{
		if (wcsncmp(L"vs", target, 2) == 0)
			return EDU_SHADER_TYPE_VERTEX;
		if (wcsncmp(L"gs", target, 2) == 0)
			return EDU_SHADER_TYPE_GEOMETRY;
		if (wcsncmp(L"ps", target, 2) == 0)
			return EDU_SHADER_TYPE_PIXEL;
		if (wcsncmp(L"cs", target, 2) == 0)
			return EDU_SHADER_TYPE_COMPUTE;
		if (wcsncmp(L"as", target, 2) == 0)
			return EDU_SHADER_TYPE_AMPLIFICATION;
		if (wcsncmp(L"ms", target, 2) == 0)
			return EDU_SHADER_TYPE_MESH;

		ASSERT_FAILED("Unexpected target: ", target);
	}

	ShaderD3D12::ShaderD3D12(const wchar_t* name,
							 const wchar_t* entryPoint,
							 const wchar_t* target,
							 const LPCWSTR* defines,
							 const ShaderDesc& desc) :
		m_Name(name)
	{
		VERIFY_EXPR(name != nullptr, "Name must not be null");
		VERIFY_EXPR(entryPoint != nullptr, "EntryPoint must not be null");
		VERIFY_EXPR(target != nullptr, "Target must not be null");

		m_Type = GetTypeFromTarget(target);

		CComPtr<IDxcUtils> pUtils;
		CComPtr<IDxcCompiler3> pCompiler;
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

		CComPtr<IDxcIncludeHandler> pIncludeHandler;
		pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

		std::vector<LPCWSTR> pszArgs =
		{
			name,
			L"-E", entryPoint,
			L"-T", target,
#if defined(DEBUG) | defined(_DEBUG)
			L"-Zi",
			L"-Qembed_debug",
#else
			L"-Fo",
			L"-O3",
			L"-Qstrip_debug",
#endif
		};

		std::vector<std::wstring> macroStr;
		if (defines)
		{
			for (uint32 i = 0; defines[i] != NULL; i += 2)
			{
				macroStr.push_back(defines[i]);
				macroStr.back().append(L"=").append(defines[i + 1]);
			}

			for (uint32 i = 0; i < macroStr.size(); i++)
			{
				pszArgs.push_back(L"-D");
				pszArgs.push_back(macroStr[i].c_str());
			}
		}

		CComPtr<IDxcBlobEncoding> pSource = nullptr;
		pUtils->LoadFile(name, nullptr, &pSource);

		if (pSource == nullptr)
		{
			ASSERT_FAILED("Shader file \"", WCharToString(name), "\" not found!");
			return;
		}

		DxcBuffer Source;
		Source.Ptr = pSource->GetBufferPointer();
		Source.Size = pSource->GetBufferSize();
		Source.Encoding = DXC_CP_ACP;

		CComPtr<IDxcResult> pResults;
		pCompiler->Compile(
			&Source,
			pszArgs.data(),
			pszArgs.size(),
			pIncludeHandler,
			IID_PPV_ARGS(&pResults)
		);

		CComPtr<IDxcBlobUtf8> pErrors = nullptr;
		pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
			LOG_ERROR("Warnings and Errors:\n", pErrors->GetStringPointer());

		HRESULT hrStatus;
		pResults->GetStatus(&hrStatus);
		if (FAILED(hrStatus))
		{
			ASSERT_FAILED("Shader Compilation Failed\n");
			return;
		}

		pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_ShaderBlob), nullptr);

		CComPtr<IDxcBlob> pReflectionData;
		pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);

		if (pReflectionData == nullptr)
		{
			ASSERT_FAILED("Failed to get shader reflection");
			return;
		}

		DxcBuffer ReflectionData;
		ReflectionData.Encoding = DXC_CP_ACP;
		ReflectionData.Ptr = pReflectionData->GetBufferPointer();
		ReflectionData.Size = pReflectionData->GetBufferSize();

		pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&m_Reflection));

		m_Resources = std::make_shared<ShaderResources>(m_Reflection, desc);
	}

	D3D12_SHADER_BYTECODE ShaderD3D12::GetShaderBytecode() const
	{
		return D3D12_SHADER_BYTECODE
		{
			reinterpret_cast<BYTE*>(m_ShaderBlob->GetBufferPointer()),
			m_ShaderBlob->GetBufferSize()
		};
	}

#ifdef EDUBINDINGDEBUG
	void ShaderD3D12::DebugPrint()
	{
		printf("---------------------------------------------------\n");
		wprintf(L"--- Shader: %s ", m_Name);
		printf(" %s ----\n", ShaderTypeStr(m_Type));
		printf("---------------------------------------------------\n");

		m_Resources->DebugPrint();
	}
#endif
}