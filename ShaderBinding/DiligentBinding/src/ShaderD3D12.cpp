#include "ShaderD3D12.h"

namespace EduEngine
{
	ShaderD3D12::ShaderD3D12(std::wstring		fileName,
							 const LPCWSTR*		defines,
							 std::wstring		entryPoint,
							 std::wstring		target,
							 RenderDeviceD3D12* device,
							 const ShaderDesc&	desc) :
		m_Desc(desc),
		m_ShaderReflection(nullptr)
	{
		// https://github.com/Microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll

		Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
		Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

		pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

		std::vector<LPCWSTR> pszArgs = {
			fileName.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", target.c_str(),
			L"-Qstrip_reflect",
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
			for (int i = 0; defines[i] != NULL; i += 2)
			{
				macroStr.emplace_back(defines[i]);
				macroStr.back().append(L"=").append(defines[i + 1]);

				pszArgs.push_back(L"-D");
				pszArgs.push_back(macroStr.back().c_str());
			}
		}

		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSource = nullptr;
		Microsoft::WRL::ComPtr<IDxcResult> pResults;

		pUtils->LoadFile(fileName.c_str(), nullptr, &pSource);
		DxcBuffer Source = {};
		Source.Ptr = pSource->GetBufferPointer();
		Source.Size = pSource->GetBufferSize();
		Source.Encoding = DXC_CP_ACP;

		pCompiler->Compile(
			&Source,
			pszArgs.data(),
			pszArgs.size(),
			pIncludeHandler.Get(),
			IID_PPV_ARGS(&pResults)
		);

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

		if (pErrors && pErrors->GetStringLength() != 0)
			OutputDebugStringA(pErrors->GetStringPointer());

		HRESULT hrStatus;
		pResults->GetStatus(&hrStatus);

		THROW_IF_FAILED(hrStatus, L"Shader Compilation Failed");

		pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_ShaderBlob), nullptr);

		//
		// Get separate reflection.
		//
		Microsoft::WRL::ComPtr<IDxcBlob> pReflectionData;
		pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);

		VERIFY_EXPR(pReflectionData != nullptr, "");

		DxcBuffer ReflectionData;
		ReflectionData.Encoding = DXC_CP_ACP;
		ReflectionData.Ptr = pReflectionData->GetBufferPointer();
		ReflectionData.Size = pReflectionData->GetBufferSize();

		auto hr = pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&m_ShaderReflection));
		THROW_IF_FAILED(hr, L"Failed to create reflection");

		m_ShaderResources = std::make_shared<ShaderResourcesD3D12>(m_ShaderReflection.Get(), desc);
		
		//
		// Initialize static layout
		//
		m_StaticCache = std::make_shared<ShaderResourceCacheD3D12>();
		m_StaticLayout = std::make_shared<ShaderResourceLayoutD3D12>();

		SHADER_VARIABLE_TYPE allowedTypes[1] = { SHADER_VARIABLE_TYPE_STATIC };
		m_StaticLayout->Initialize(device->GetD3D12Device(), m_ShaderResources, allowedTypes, 1, m_StaticCache.get(), nullptr, true);

#ifdef _DEBUG
		m_FileName = fileName;
		m_EntryPoint = entryPoint;
#endif
	}

	D3D12_SHADER_BYTECODE ShaderD3D12::GetShaderBytecode() const
	{
		return D3D12_SHADER_BYTECODE
		{
			reinterpret_cast<BYTE*>(m_ShaderBlob->GetBufferPointer()),
			m_ShaderBlob->GetBufferSize()
		};
	}

#ifdef _DEBUG
	void ShaderD3D12::DebugPrint()
	{
		printf("=================================================================\n");
		wprintf(L"========== Shader \"%s\" |%s| ==========\n", m_FileName.c_str(), m_EntryPoint.c_str());
		printf("=================================================================\n\n");
		m_ShaderResources->DebugPrint();
		m_StaticLayout->DebugPrint();
		m_StaticCache->DebugPrint();
		printf("\n\n");
	}
#endif
}
