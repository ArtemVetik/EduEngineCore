#pragma once

#include "framework.h"
#include "RootSignatureD3D12.h"

#include "../../Common/include/SimpleMath.h"

#include <PipelineStateD3D12.h>
#include <PipelineState.h>

namespace EduEngine
{
	using namespace DirectX;

	class ForwardOpaque
	{
	public:
		struct ObjectConstants
		{
			XMFLOAT4X4 World;
		};

		struct PassConstants
		{
			XMFLOAT4X4 ViewProj;
			XMFLOAT4 AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
			UINT DirectionalLightsCount = 1;
			XMFLOAT3 CamPos = { 0, 0, 0 };
		};

		struct MaterialConstants
		{
			XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			XMFLOAT3 FresnelR0 = { 0.05f, 0.05f, 0.05f };
			float Roughness = 0.15f;
		};

		struct Light
		{
		public:
			enum Type
			{
				Directional = 0,
				Point = 1,
				Spotlight = 2
			};

			Type LightType = Type::Directional;
			DirectX::XMFLOAT3 Padding = { 0, 0, 0 };
			DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
			float FalloffStart = 1.04f;							 // point/spot light only
			DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f }; // directional/spot light only
			float FalloffEnd = 10.0f;							 // point/spot light only
			DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };	 // point/spot light only
			float SpotPower = 64.0f;							 // spot light only
		};

	private:

		std::shared_ptr<EduEngine::DiligentBinding::ShaderD3D12> m_VertexShader;
		std::shared_ptr<EduEngine::DiligentBinding::ShaderD3D12> m_PixelShader;
		EduEngine::DiligentBinding::ShaderDesc m_psDesc;
		EduEngine::DiligentBinding::ShaderDesc m_vsDesc;
		EduEngine::DiligentBinding::StaticSamplerDesc m_psStaticSamplers[1];

		EduEngine::DiligentBinding::PipelineStateD3D12 m_Pso;

	public:
		ForwardOpaque(RenderDeviceD3D12* device, const LPCWSTR* macros = nullptr)
		{
			m_psStaticSamplers[0].Desc = CD3DX12_STATIC_SAMPLER_DESC(0,
				D3D12_FILTER_MIN_MAG_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				0.0f,
				8);
			m_psStaticSamplers[0].TextureName = "gAlbedo";

			EduEngine::DiligentBinding::ShaderVariableDesc vsVars[]{
				EduEngine::DiligentBinding::ShaderVariableDesc("cbPerObject", EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_DYNAMIC),
				EduEngine::DiligentBinding::ShaderVariableDesc("cbPerPass", EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_DYNAMIC)
			};

			EduEngine::DiligentBinding::ShaderVariableDesc psVars[]{
				EduEngine::DiligentBinding::ShaderVariableDesc("cbMaterial", EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_MUTABLE),
				EduEngine::DiligentBinding::ShaderVariableDesc("gLight", EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_DYNAMIC),
				EduEngine::DiligentBinding::ShaderVariableDesc("gAlbedo", EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_STATIC),
			};

			m_psDesc = {};
			m_psDesc.ShaderType = EduEngine::DiligentBinding::EDU_SHADER_TYPE_PIXEL;
			m_psDesc.DefaultVarType = EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_MUTABLE;
			m_psDesc.NumVarDesc = _countof(psVars);
			m_psDesc.VarDesc = psVars;
			m_psDesc.NumStaticSamplers = _countof(m_psStaticSamplers);
			m_psDesc.StaticSamplers = m_psStaticSamplers;

			m_vsDesc = {};
			m_vsDesc.ShaderType = EduEngine::DiligentBinding::EDU_SHADER_TYPE_VERTEX;
			m_vsDesc.DefaultVarType = EduEngine::DiligentBinding::SHADER_VARIABLE_TYPE_MUTABLE;
			m_vsDesc.NumVarDesc = _countof(vsVars);
			m_vsDesc.VarDesc = vsVars;
			m_vsDesc.NumStaticSamplers = 0;

			m_VertexShader = std::make_shared<EduEngine::DiligentBinding::ShaderD3D12>(L"Shaders\\Opaque.hlsl", macros, L"VS", L"vs_6_0", device, m_vsDesc);
			m_PixelShader = std::make_shared<EduEngine::DiligentBinding::ShaderD3D12>(L"Shaders\\Opaque.hlsl", macros, L"PS", L"ps_6_0", device, m_psDesc);
		}

		EduEngine::DiligentBinding::ShaderResourceLayoutD3D12::SRV_CBV_UAV& GetStaticVSVariable(const char* name) { return m_VertexShader->GetStaticVariable(name); }
		EduEngine::DiligentBinding::ShaderResourceLayoutD3D12::SRV_CBV_UAV& GetStaticPSVariable(const char* name) { return m_PixelShader->GetStaticVariable(name); }

		void Build(RenderDeviceD3D12* device)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			D3D12_DEPTH_STENCIL_DESC dsDesc = {};
			dsDesc.DepthEnable = TRUE;
			dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetShader(m_VertexShader);
			m_Pso.SetShader(m_PixelShader);
			m_Pso.SetDepthStencilState(dsDesc);
			m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			m_Pso.Build(device);
			m_Pso.SetName(L"ColorPSO");
		}

		EduEngine::DiligentBinding::PipelineStateD3D12& GetPipelineState() { return m_Pso; }
	};

	class PBRLighting
	{
	public:
		struct ObjectConstants
		{
			XMFLOAT4X4 World;
		};

		struct PassConstants
		{
			XMFLOAT4X4 ViewProj;
			UINT DirectionalLightsCount = 1;
			XMFLOAT3 CamPos = { 0, 0, 0 };
		};

		struct MaterialConstants
		{
			XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		};

		struct Light
		{
		public:
			enum Type
			{
				Directional = 0,
				Point = 1,
				Spotlight = 2
			};

			Type LightType = Type::Directional;
			DirectX::XMFLOAT3 Padding = { 0, 0, 0 };
			DirectX::XMFLOAT3 Strength = { 0.9f, 0.9f, 0.9f };
			float FalloffStart = 1.04f;							 // point/spot light only
			DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f }; // directional/spot light only
			float FalloffEnd = 10.0f;							 // point/spot light only
			DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };	 // point/spot light only
			float SpotPower = 64.0f;							 // spot light only
		};

	private:
		std::shared_ptr<EduEngine::EduBinding::ShaderD3D12> m_VertexShader;
		std::shared_ptr<EduEngine::EduBinding::ShaderD3D12> m_PixelShader;
		EduEngine::EduBinding::PipelineState m_Pso;
		EduEngine::EduBinding::ShaderDesc m_psDesc;
		EduEngine::EduBinding::ShaderDesc m_vsDesc;

	public:
		PBRLighting(RenderDeviceD3D12* device, const LPCWSTR* macros = nullptr)
		{
			EduEngine::EduBinding::ShaderResourceDesc vsVars[]{
				EduEngine::EduBinding::ShaderResourceDesc("cbPerObject", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_DYNAMIC),
				EduEngine::EduBinding::ShaderResourceDesc("cbPerPass", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_DYNAMIC)
			};

			EduEngine::EduBinding::ShaderResourceDesc psVars[]{
				EduEngine::EduBinding::ShaderResourceDesc("cbPerPass", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_DYNAMIC),
				EduEngine::EduBinding::ShaderResourceDesc("cbMaterial", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE),
				EduEngine::EduBinding::ShaderResourceDesc("gLight", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_DYNAMIC),
				EduEngine::EduBinding::ShaderResourceDesc("gAlbedo", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE),
				EduEngine::EduBinding::ShaderResourceDesc("gMetallicRoughness", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE),
				EduEngine::EduBinding::ShaderResourceDesc("gAO", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE),
				EduEngine::EduBinding::ShaderResourceDesc("gNormalMap", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE),
			};

			m_vsDesc.DefaultType = EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE;
			m_vsDesc.ResourceNum = _countof(vsVars);
			m_vsDesc.ResourceDesc = vsVars;

			m_psDesc.DefaultType = EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE;
			m_psDesc.ResourceNum = _countof(psVars);
			m_psDesc.ResourceDesc = psVars;

			m_VertexShader = std::make_shared<EduEngine::EduBinding::ShaderD3D12>(L"Shaders\\PBRLighting.hlsl", L"VS", L"vs_6_0", macros, m_vsDesc);
			m_PixelShader = std::make_shared<EduEngine::EduBinding::ShaderD3D12>(L"Shaders\\PBRLighting.hlsl", L"PS", L"ps_6_0", macros, m_psDesc);
		}

		void Build(RenderDeviceD3D12* device)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			D3D12_DEPTH_STENCIL_DESC dsDesc = {};
			dsDesc.DepthEnable = TRUE;
			dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetShader(m_VertexShader);
			m_Pso.SetShader(m_PixelShader);
			m_Pso.SetDepthStencilState(dsDesc);
			m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);

			m_Pso.Build(device);
		}

		EduEngine::EduBinding::PipelineState& GetPipelineState() { return m_Pso; }
	};

	class DebugRenderPass
	{
	public:
		struct PassConstants
		{
			DirectX::XMFLOAT4X4 MVP;
			DirectX::XMFLOAT3 CamPos;
			float Padding;
		};

	private:
		std::shared_ptr<EduEngine::EduBinding::ShaderD3D12> m_VertexShader;
		std::shared_ptr<EduEngine::EduBinding::ShaderD3D12> m_PixelShader;
		EduEngine::EduBinding::PipelineState m_Pso;

	public:
		DebugRenderPass(RenderDeviceD3D12* device)
		{
			EduEngine::EduBinding::ShaderResourceDesc vsVars[]{
				EduEngine::EduBinding::ShaderResourceDesc("cbPass", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_DYNAMIC)
			};

			EduEngine::EduBinding::ShaderResourceDesc psVars[]{
				EduEngine::EduBinding::ShaderResourceDesc("cbPass", EduEngine::EduBinding::SHADER_RESOURCE_TYPE_DYNAMIC)
			};

			EduEngine::EduBinding::ShaderDesc vsDesc = {};
			vsDesc.DefaultType = EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE;
			vsDesc.ResourceNum = _countof(vsVars);
			vsDesc.ResourceDesc = vsVars;

			EduEngine::EduBinding::ShaderDesc psDesc;
			psDesc.DefaultType = EduEngine::EduBinding::SHADER_RESOURCE_TYPE_MUTABLE;
			psDesc.ResourceNum = _countof(psVars);
			psDesc.ResourceDesc = psVars;

			m_VertexShader = std::make_shared<EduEngine::EduBinding::ShaderD3D12>(L"Shaders\\DebugRender.hlsl", L"VS", L"vs_6_0", nullptr, vsDesc);
			m_PixelShader = std::make_shared<EduEngine::EduBinding::ShaderD3D12>(L"Shaders\\DebugRender.hlsl", L"PS", L"ps_6_0", nullptr, psDesc);

			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	 0,  0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			D3D12_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			auto dss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			dss.DepthEnable = false;
			dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

			m_Pso.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
			m_Pso.SetBlendState(blendDesc);
			m_Pso.SetDepthStencilState(dss);
			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetShader(m_VertexShader);
			m_Pso.SetShader(m_PixelShader);
			m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			m_Pso.Build(device);
		}

		EduEngine::EduBinding::PipelineState& GetPipelineState() { return m_Pso; }
	};
}