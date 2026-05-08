#include "RootSignature.h"
#include "ShadersLayoutBuilder.h"
#include "DebugEnumPrint.h"

#include <Asserts.h>

namespace EduEngine::EduBinding
{
	__forceinline D3D12_SHADER_VISIBILITY GetVisibility(EDU_SHADER_TYPE shaderType)
	{
		switch (shaderType)
		{
		case EDU_SHADER_TYPE_VERTEX: return D3D12_SHADER_VISIBILITY_VERTEX;
		case EDU_SHADER_TYPE_HULL: return D3D12_SHADER_VISIBILITY_HULL;
		case EDU_SHADER_TYPE_DOMAIN: return D3D12_SHADER_VISIBILITY_DOMAIN;
		case EDU_SHADER_TYPE_GEOMETRY: return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case EDU_SHADER_TYPE_PIXEL: return D3D12_SHADER_VISIBILITY_PIXEL;
		case EDU_SHADER_TYPE_AMPLIFICATION: return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
		case EDU_SHADER_TYPE_MESH: return D3D12_SHADER_VISIBILITY_MESH;
		case EDU_SHADER_TYPE_COMPUTE: return D3D12_SHADER_VISIBILITY_ALL;
		default: ASSERT_FAILED("Invalid shader type");
		}
	}

	void RootSignature::Build(RenderDeviceD3D12* device,
							  ShaderD3D12** shaders,
							  uint8 shadersNum,
							  D3D12_STATIC_SAMPLER_DESC* overrideStaticSamplers /* = nullptr */,
							  uint32 numOverrideStaticSamplers /* = 0*/)
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE rsFeature = {};
		rsFeature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		// TODO: if root signature version 1.1 doesn't suport, use 1.0
		if (FAILED(device->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsFeature, sizeof(rsFeature))))
		{
			ASSERT_FAILED("Device doesn't support D3D_ROOT_SIGNATURE_VERSION_1_1");
			return;
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS opts = {};
		if (FAILED(device->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts))))
		{
			ASSERT_FAILED("Failed to fetch D3D12_FEATURE_D3D12_OPTIONS");
		}

		auto CreateTableRange = [](ShaderResourceInfo& resInfo, D3D12_DESCRIPTOR_RANGE_TYPE rangeType)
			{
				D3D12_DESCRIPTOR_RANGE1 range;
				range.RangeType = rangeType;
				range.BaseShaderRegister = resInfo.GetBindPoint();
				range.RegisterSpace = resInfo.GetSpace();
				range.NumDescriptors = resInfo.GetBindCount();
				range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				return range;
			};

		uint8 rootIndex = 0;

#ifndef EDUBINDINGDEBUG
		D3D12_ROOT_PARAMETER1 params[64];
		std::vector<D3D12_DESCRIPTOR_RANGE1> tableRanges[SHADER_RESOURCE_TYPE_NUM][EDU_SHADER_TYPE_NUM];
#endif

		SHADER_RESOURCE_TYPE resType;
		EDU_SHADER_TYPE shaderType;

		ProcessShadersLayout(shaders, shadersNum,
			[&](uint8 rootViewsNum, uint8 descriptorTablesNum, uint8 descriptorsNum)
			{

			},
			[&](SHADER_RESOURCE_TYPE ResType, EDU_SHADER_TYPE ShaderType) // OnShaderStart
			{
				resType = ResType;
				shaderType = ShaderType;
			},
			[&](ShaderResourceInfo& cb)
			{
				D3D12_ROOT_PARAMETER1 param = {};
				param.Descriptor.ShaderRegister = cb.GetBindPoint();
				param.Descriptor.RegisterSpace = cb.GetSpace();
				param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				param.ShaderVisibility = GetVisibility(shaderType);

				params[rootIndex++] = param;
			},
			[&](ShaderResourceInfo& texSrv)
			{
				tableRanges[resType][shaderType].push_back(CreateTableRange(texSrv, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
			},
			[&](ShaderResourceInfo& buffSrv)
			{
				tableRanges[resType][shaderType].push_back(CreateTableRange(buffSrv, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
			},
			[&](ShaderResourceInfo& texUAV)
			{
				tableRanges[resType][shaderType].push_back(CreateTableRange(texUAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));
			},
			[&](ShaderResourceInfo& buffUAV)
			{
				tableRanges[resType][shaderType].push_back(CreateTableRange(buffUAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));
			},
			[&](uint8 cbNum, uint8 descriptorsNum) // OnShaderEnd
			{
				if (tableRanges[resType][shaderType].size())
				{
					D3D12_ROOT_PARAMETER1 tableParam = {};
					tableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					tableParam.DescriptorTable.pDescriptorRanges = tableRanges[resType][shaderType].data();
					tableParam.DescriptorTable.NumDescriptorRanges = tableRanges[resType][shaderType].size();
					tableParam.ShaderVisibility = GetVisibility(shaderType);

					params[rootIndex++] = tableParam;
				}
			}
		);

#ifndef EDUBINDINGDEBUG
		D3D12_STATIC_SAMPLER_DESC staticSamplers[16];
#endif
		uint32 samplersNum = 7;

		if (overrideStaticSamplers)
		{
			memcpy(staticSamplers, overrideStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * numOverrideStaticSamplers);
			samplersNum = numOverrideStaticSamplers;
		}
		else
		{
			InitStaticSamplers(staticSamplers);
		}

#ifndef EDUBINDINGDEBUG
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
#else
		rootSigDesc = {};
#endif

		rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		if (opts.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_3)
			rootSigDesc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		else
			LOG_ERROR("Device doesn't support D3D12_RESOURCE_BINDING_TIER_3. It's not possible to use bindless rendering");

		rootSigDesc.Desc_1_1.NumStaticSamplers = samplersNum;
		rootSigDesc.Desc_1_1.pStaticSamplers = staticSamplers;
		rootSigDesc.Desc_1_1.pParameters = params;
		rootSigDesc.Desc_1_1.NumParameters = rootIndex;

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

		HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSigDesc, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr)
			LOG_ERROR((char*)errorBlob->GetBufferPointer());

		THROW_IF_FAILED(hr, L"Failed to serialize root signature");

		hr = device->GetD3D12Device()->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(&m_RootSignature)
		);

		THROW_IF_FAILED(hr, L"Failed to create root signature");

		m_RootSignature->SetName(L"Root Signature");
	}

#ifdef EDUBINDINGDEBUG
	void RootSignature::DebugPrint()
	{
		printf("----------------------------------------\n");
		printf("------------ Root Signature ------------\n");
		printf("----------------------------------------\n");

		auto rootSignatureDesc = rootSigDesc.Desc_1_1;

		printf("Num Parameters: %u\n", rootSignatureDesc.NumParameters);

		for (uint32 i = 0; i < rootSignatureDesc.NumParameters; i++)
		{
			auto p = rootSignatureDesc.pParameters[i];

			printf("%u: Type: %s\tVisibility: %s\n", i, ParamTypeStr(p.ParameterType), SdrVisStr(p.ShaderVisibility));

			if (p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				for (uint32 j = 0; j < p.DescriptorTable.NumDescriptorRanges; j++)
				{
					auto d = p.DescriptorTable.pDescriptorRanges[j];
					printf("\t%u: BaseShaderRegister: %u\tNumDescriptors: %u\tOffsetFromStart: %u\tRangeType: %s\tRegisterSpace: %u\n",
						j, d.BaseShaderRegister, d.NumDescriptors, d.OffsetInDescriptorsFromTableStart, RangeTypeStr(d.RangeType), d.RegisterSpace);
				}
			}
			else
			{
				printf("\tShaderRegister: %d, RegisterSpace: %d\n", p.Descriptor.ShaderRegister, p.Descriptor.RegisterSpace);
			}
		}

		printf("\nNum Static Samplers: %u\n", rootSignatureDesc.NumStaticSamplers);

		for (uint32 i = 0; i < rootSignatureDesc.NumStaticSamplers; i++)
		{
			auto s = rootSignatureDesc.pStaticSamplers[i];

			printf("Register %u\tRegister Space %u\tVisibility %s\tFilter: %s\n", s.ShaderRegister, s.RegisterSpace, SdrVisStr(s.ShaderVisibility), FilterStr(s.Filter));
		}

		printf("\n\n");
	}
#endif

	void RootSignature::InitStaticSamplers(D3D12_STATIC_SAMPLER_DESC* staticSamplers)
	{
		for (uint8 i = 0; i < 7; i++)
		{
			staticSamplers[i].MipLODBias = 0;
			staticSamplers[i].MaxAnisotropy = 16;
			staticSamplers[i].MinLOD = 0.0f;
			staticSamplers[i].MaxLOD = D3D12_FLOAT32_MAX;
			staticSamplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			staticSamplers[i].RegisterSpace = 0;
		}

		staticSamplers[0].ShaderRegister = 0;
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		staticSamplers[1].ShaderRegister = 1;
		staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		staticSamplers[2].ShaderRegister = 2;
		staticSamplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSamplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		staticSamplers[3].ShaderRegister = 3;
		staticSamplers[3].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSamplers[3].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[3].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[3].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		staticSamplers[4].ShaderRegister = 4;
		staticSamplers[4].Filter = D3D12_FILTER_ANISOTROPIC;
		staticSamplers[4].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[4].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[4].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[4].MipLODBias = 0.0f;
		staticSamplers[4].MaxAnisotropy = 8;

		staticSamplers[5].ShaderRegister = 5;
		staticSamplers[5].Filter = D3D12_FILTER_ANISOTROPIC;
		staticSamplers[5].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[5].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[5].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[5].MipLODBias = 0.0f;
		staticSamplers[5].MaxAnisotropy = 8;

		staticSamplers[6].ShaderRegister = 6;
		staticSamplers[6].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		staticSamplers[6].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSamplers[6].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSamplers[6].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSamplers[6].MipLODBias = 0.0f;
		staticSamplers[6].MaxAnisotropy = 16;
		staticSamplers[6].ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		staticSamplers[6].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	}
}