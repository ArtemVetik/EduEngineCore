#pragma once

#include "framework.h"
#include "RootSignatureD3D12.h"

#include "../../Common/include/SimpleMath.h"
#include "../../Graphics/include/PipelineStateD3D12.h"

namespace EduEngine
{
	class ColorPass
	{
	public:
		struct ObjectConstants
		{
			DirectX::XMFLOAT4X4 World;
		};

		struct PassConstants
		{
			DirectX::XMFLOAT4X4 ViewProj;
		};

	private:
		std::shared_ptr<ShaderD3D12> m_VertexShader;
		std::shared_ptr<ShaderD3D12> m_PixelShader;
		ShaderDesc m_psDesc;
		ShaderDesc m_vsDesc;
		StaticSamplerDesc m_psStaticSamplers[1];

		PipelineStateD3D12 m_Pso;

	public:
		ColorPass(RenderDeviceD3D12* device, const LPCWSTR* macros = nullptr)
		{
			m_psStaticSamplers[0].Desc = CD3DX12_STATIC_SAMPLER_DESC(0,
				D3D12_FILTER_MIN_MAG_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				0.0f,
				8);
			m_psStaticSamplers[0].TextureName = "gAlbedo";

			ShaderVariableDesc vsVars[] {
				ShaderVariableDesc("cbPerObject", SHADER_VARIABLE_TYPE_DYNAMIC),
				ShaderVariableDesc("cbPerPass", SHADER_VARIABLE_TYPE_DYNAMIC)
			};

			ShaderVariableDesc psVars[] {
				ShaderVariableDesc("gAlbedo", SHADER_VARIABLE_TYPE_STATIC)
			};

			m_psDesc = {};
			m_psDesc.ShaderType = EDU_SHADER_TYPE_PIXEL;
			m_psDesc.DefaultVarType = SHADER_VARIABLE_TYPE_MUTABLE;
			m_psDesc.NumVarDesc = _countof(psVars);
			m_psDesc.VarDesc = psVars;
			m_psDesc.NumStaticSamplers = _countof(m_psStaticSamplers);
			m_psDesc.StaticSamplers = m_psStaticSamplers;

			m_vsDesc = {};
			m_vsDesc.ShaderType = EDU_SHADER_TYPE_VERTEX;
			m_vsDesc.DefaultVarType = SHADER_VARIABLE_TYPE_MUTABLE;
			m_vsDesc.NumVarDesc = _countof(vsVars);
			m_vsDesc.VarDesc = vsVars;
			m_vsDesc.NumStaticSamplers = 0;

			m_VertexShader = std::make_shared<ShaderD3D12>(L"Shaders\\Opaque.hlsl", macros, L"VS", L"vs_6_0", device, m_vsDesc);
			m_PixelShader = std::make_shared<ShaderD3D12>(L"Shaders\\Opaque.hlsl", macros, L"PS", L"ps_6_0", device, m_psDesc);
		}

		ShaderResourceLayoutD3D12::SRV_CBV_UAV& GetStaticVSVariable(const char* name) { return m_VertexShader->GetStaticVariable(name); }
		ShaderResourceLayoutD3D12::SRV_CBV_UAV& GetStaticPSVariable(const char* name) { return m_PixelShader->GetStaticVariable(name); }

		void Build(RenderDeviceD3D12* device)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetShader(m_VertexShader);
			m_Pso.SetShader(m_PixelShader);
			m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			m_Pso.Build(device);
			m_Pso.SetName(L"ColorPSO");
		}

		PipelineStateD3D12& GetPipelineState() { return m_Pso; }
	};
}