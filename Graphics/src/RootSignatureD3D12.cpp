#include "RootSignatureD3D12.h"
#include "CommandContext.h"
#include "DebugEnumPrint.h"

namespace EduEngine
{
	RootSignatureD3D12_1::RootSignatureD3D12_1() :
		m_RootParams(),
		m_StaticSamplers(),
		m_DynamicSignature(false)
	{
		for (size_t s = 0; s < SHADER_VARIABLE_TYPE_NUM_TYPES; ++s)
		{
			m_TotalSrvCbvUavSlots[s] = 0;
			m_TotalSamplerSlots[s] = 0;
		}

		for (size_t i = 0; i < _countof(m_SrvCbvUavRootTablesMap); ++i)
			m_SrvCbvUavRootTablesMap[i] = InvalidRootTableIndex;
		for (size_t i = 0; i < _countof(m_SamplerRootTablesMap); ++i)
			m_SamplerRootTablesMap[i] = InvalidRootTableIndex;
	}

	void RootSignatureD3D12_1::AllocateResourceSlot(EDU_SHADER_TYPE              shaderType,
		const ShaderResourceAttribs& shaderResAttribs,
		D3D12_DESCRIPTOR_RANGE_TYPE  rangeType,
		uint32& rootIndexOut,
		uint32& offsetFromTableStartOut)
	{
		int shaderInd = shaderType;
		auto shaderVisibility = GetShaderVisibility(shaderType);
		if (rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV && shaderResAttribs.BindCount == 1)
		{
			rootIndexOut = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
			offsetFromTableStartOut = 0;

			m_RootParams.AddRootView(D3D12_ROOT_PARAMETER_TYPE_CBV, rootIndexOut, shaderResAttribs.BindPoint, shaderVisibility, shaderResAttribs.GetVarType());
		}
		else
		{
			auto rootTableType = (shaderResAttribs.GetVarType() == SHADER_VARIABLE_TYPE_DYNAMIC) ? SHADER_VARIABLE_TYPE_DYNAMIC : SHADER_VARIABLE_TYPE_STATIC;
			auto tableIndKey = shaderInd * SHADER_VARIABLE_TYPE_NUM_TYPES + rootTableType;
			auto& rootTableInd = ((rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ? m_SamplerRootTablesMap : m_SrvCbvUavRootTablesMap)[tableIndKey];
			if (rootTableInd == InvalidRootTableIndex)
			{
				rootIndexOut = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
				VERIFY_EXPR(m_RootParams.GetNumRootTables() < 255, "");
				rootTableInd = static_cast<uint8>(m_RootParams.GetNumRootTables());
				m_RootParams.AddRootTable(rootIndexOut, shaderVisibility, rootTableType, 1);
			}
			else
			{
				m_RootParams.AddDescriptorRanges(rootTableInd, 1);
			}

			auto& currParam = m_RootParams.GetRootTable(rootTableInd);
			rootIndexOut = currParam.GetRootIndex();

			const auto& d3d12RootParam = static_cast<const D3D12_ROOT_PARAMETER&>(currParam);
			VERIFY_EXPR(d3d12RootParam.ShaderVisibility == shaderVisibility, "Shader visibility is not correct");

			offsetFromTableStartOut = currParam.GetDescriptorTableSize();

			uint32 newDescriptorRangeIndex = d3d12RootParam.DescriptorTable.NumDescriptorRanges - 1;
			currParam.SetDescriptorRange(newDescriptorRangeIndex, rangeType, shaderResAttribs.BindPoint, shaderResAttribs.BindCount, 0, offsetFromTableStartOut);
		}
	}

	void RootSignatureD3D12_1::AllocateStaticSamplers(const ShaderD3D12** shaders, uint32 numShaders)
	{
		VERIFY_EXPR(m_StaticSamplers.size() == 0, "Static samplers already allocated");

		uint32 totalSamplers = 0;
		for (uint32 i = 0; i < numShaders; ++i)
			totalSamplers += shaders[i]->GetDesc().NumStaticSamplers;

		if (totalSamplers > 0)
		{
			m_StaticSamplers.reserve(totalSamplers);
			for (uint32 i = 0; i < numShaders; ++i)
			{
				const auto& desc = shaders[i]->GetDesc();
				for (uint32 sam = 0; sam < desc.NumStaticSamplers; ++sam)
				{
					m_StaticSamplers.emplace_back(desc.StaticSamplers[sam], GetShaderVisibility(desc.ShaderType));
				}
			}

			VERIFY_EXPR(m_StaticSamplers.size() == totalSamplers, "");
		}
	}

	void RootSignatureD3D12_1::InitStaticSampler(EDU_SHADER_TYPE shaderType, const String& textureName, const ShaderResourceAttribs& samplerAttribs)
	{
		auto shaderVisibility = GetShaderVisibility(shaderType);
		auto samplerFound = false;
		for (auto& stSmplr : m_StaticSamplers)
		{
			if (stSmplr.SamplerDesc.Desc.ShaderVisibility == shaderVisibility && textureName.compare(stSmplr.SamplerDesc.TextureName) == 0)
			{
				stSmplr.SamplerDesc.Desc.ShaderRegister = samplerAttribs.BindPoint;
				stSmplr.SamplerDesc.Desc.RegisterSpace = 0;
				stSmplr.ArraySize = samplerAttribs.BindCount;
				samplerFound = true;
				break;
			}
		}

		if (!samplerFound)
		{
			ASSERT_FAILED("Failed to find static sampler");
		}
	}

	void RootSignatureD3D12_1::CommitRootViews(ShaderResourceCacheD3D12& resourceCache, CommandContext* ctx, bool isCompute) const
	{
		for (uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
		{
			auto& rootView = m_RootParams.GetRootView(rv);
			auto rootInd = rootView.GetRootIndex();

			auto& res = resourceCache.GetRootTable(rootInd).GetResource(0);

			if (res.pObject)
			{
				if (!res.pObject->CheckAllStates(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
					ctx->TransitionResource(res.pObject.get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			}

			D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = res.pObject ?
				res.pObject->GetD3D12Resource()->GetGPUVirtualAddress() :
				res.pDynObject->GetAllocation().GPUAddress;

			if (isCompute)
				ctx->GetCmdList()->SetComputeRootConstantBufferView(rootInd, cbvAddress);
			else
				ctx->GetCmdList()->SetGraphicsRootConstantBufferView(rootInd, cbvAddress);
		}
	}

	void RootSignatureD3D12_1::CommitDescriptorHandles(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache, CommandContext* ctx, bool isCompute, bool transitionResources) const
	{
		if (m_DynamicSignature)
			CommitDescriptorHandlesInternal_SMD(device, resourceCache, ctx, isCompute, transitionResources);
		else
			CommitDescriptorHandlesInternal_SM(device, resourceCache, ctx, isCompute, transitionResources);
	}

	void RootSignatureD3D12_1::InitResourceCache(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache)
	{
		std::vector<uint32> cacheTableSizes(m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews(), 0);

		for (uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
		{
			auto& rootParam = m_RootParams.GetRootTable(rt);
			cacheTableSizes[rootParam.GetRootIndex()] = rootParam.GetDescriptorTableSize();
		}
		for (uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
		{
			auto& rootParam = m_RootParams.GetRootView(rv);
			cacheTableSizes[rootParam.GetRootIndex()] = 1;
		}

		resourceCache.Initialize(static_cast<uint32>(cacheTableSizes.size()), cacheTableSizes.data());

		uint32 totalSrvCbvUavDescriptors =
			m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_STATIC] +
			m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_MUTABLE];
		uint32 totalSamplerDescriptors =
			m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_STATIC] +
			m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_MUTABLE];

		DescriptorHeapAllocation cbcSrvUavHeapSpace, samplerHeapSpace;
		if (totalSrvCbvUavDescriptors)
			cbcSrvUavHeapSpace = device->AllocateGPUDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, totalSrvCbvUavDescriptors);
		// TODO: set QueueID
		if (totalSamplerDescriptors)
			samplerHeapSpace = device->AllocateGPUDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, totalSamplerDescriptors);

		uint32 SrvCbvUavTblStartOffset = 0;
		uint32 SamplerTblStartOffset = 0;
		for (uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
		{
			auto& rootParam = m_RootParams.GetRootTable(rt);
			const auto& d3d12RootParam = static_cast<const D3D12_ROOT_PARAMETER&>(rootParam);
			auto& rootTableCache = resourceCache.GetRootTable(rootParam.GetRootIndex());

			VERIFY_EXPR(d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "");

			auto tableSize = rootParam.GetDescriptorTableSize();
			VERIFY_EXPR(tableSize > 0, "Unexpected empty descriptor table");

			auto HeapType = d3d12RootParam.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
				? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
				: D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

			// Space for dynamic variables is allocated at every draw call
			if (rootParam.GetShaderVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
			{
				if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
				{
					rootTableCache.TableStartOffset = SrvCbvUavTblStartOffset;
					SrvCbvUavTblStartOffset += tableSize;
				}
				else
				{
					rootTableCache.TableStartOffset = SamplerTblStartOffset;
					SamplerTblStartOffset += tableSize;
				}
			}
			else
			{
				VERIFY_EXPR(rootTableCache.TableStartOffset == ShaderResourceCacheD3D12::InvalidDescriptorOffset, "");
			}
		}

		VERIFY_EXPR(SrvCbvUavTblStartOffset == totalSrvCbvUavDescriptors, "");
		VERIFY_EXPR(SamplerTblStartOffset == totalSamplerDescriptors, "");

		resourceCache.SetDescriptorHeapSpace(std::move(cbcSrvUavHeapSpace), std::move(samplerHeapSpace));
	}

	void RootSignatureD3D12_1::Build(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache)
	{
		for (uint32 i = 0; i < m_RootParams.GetNumRootTables(); ++i)
		{
			auto& rootTbl = m_RootParams.GetRootTable(i);
			auto& d3d12RootParam = static_cast<const D3D12_ROOT_PARAMETER&>(rootTbl);
			VERIFY_EXPR(d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "");

			auto tableSize = rootTbl.GetDescriptorTableSize();
			VERIFY_EXPR(d3d12RootParam.DescriptorTable.NumDescriptorRanges > 0 && tableSize > 0, "Unexpected empty descriptor table");
			auto isSamplerTable = d3d12RootParam.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			auto varType = rootTbl.GetShaderVariableType();
			(isSamplerTable ? m_TotalSamplerSlots : m_TotalSrvCbvUavSlots)[varType] += tableSize;
		}

		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		auto totalParams = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
		m_RootParamsD3D12 = std::vector<D3D12_ROOT_PARAMETER>(totalParams, D3D12_ROOT_PARAMETER());
		for (uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
		{
			auto& rootTable = m_RootParams.GetRootTable(rt);
			const D3D12_ROOT_PARAMETER& srcParam = rootTable;
			VERIFY_EXPR(srcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && srcParam.DescriptorTable.NumDescriptorRanges > 0, "Non-empty descriptor table is expected");
			m_RootParamsD3D12[rootTable.GetRootIndex()] = srcParam;
		}
		for (uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
		{
			auto& rootView = m_RootParams.GetRootView(rv);
			const D3D12_ROOT_PARAMETER& srcParam = rootView;
			VERIFY_EXPR(srcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV, "Root CBV is expected");
			m_RootParamsD3D12[rootView.GetRootIndex()] = srcParam;
		}

		rootSignatureDesc.NumParameters = static_cast<UINT>(m_RootParamsD3D12.size());
		rootSignatureDesc.pParameters = m_RootParamsD3D12.size() ? m_RootParamsD3D12.data() : nullptr;

		UINT totalD3D12StaticSamplers = 0;
		for (const auto& stSam : m_StaticSamplers)
			totalD3D12StaticSamplers += stSam.ArraySize;

		rootSignatureDesc.NumStaticSamplers = totalD3D12StaticSamplers;
		rootSignatureDesc.pStaticSamplers = nullptr;

		m_d3d12StaticSamplers.reserve(totalD3D12StaticSamplers);

		if (!m_StaticSamplers.empty())
		{
			for (size_t s = 0; s < m_StaticSamplers.size(); ++s)
			{
				auto& stSmplrDesc = m_StaticSamplers[s];
				auto& samDesc = stSmplrDesc.SamplerDesc.Desc;

				for (uint32 arrInd = 0; arrInd < stSmplrDesc.ArraySize; arrInd++)
				{
					samDesc.ShaderRegister += arrInd;
					m_d3d12StaticSamplers.emplace_back(samDesc);
				}
			}

			rootSignatureDesc.pStaticSamplers = m_d3d12StaticSamplers.data();

			// Release static samplers array, we no longer need it
			std::vector<StaticSamplerAttribs> emptySamplers;
			m_StaticSamplers.swap(emptySamplers);

			VERIFY_EXPR(m_d3d12StaticSamplers.size() == totalD3D12StaticSamplers, "");
		}

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		hr = device->GetD3D12Device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(m_d3d12RootSignature), reinterpret_cast<void**>(static_cast<ID3D12RootSignature**>(&m_d3d12RootSignature)));
		THROW_IF_FAILED(hr, L"Failed to create root signature");

		m_DynamicSignature = m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_DYNAMIC] > 0 || m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_DYNAMIC] > 0;

		InitResourceCache(device, resourceCache);
	}

#ifdef _DEBUG
	void RootSignatureD3D12_1::DebugPrint()
	{
		printf("----------------------------------------\n");
		printf("------------ Root Signature ------------\n");
		printf("----------------------------------------\n");

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

		printf("----------------------------------------\n\n");
	}
#endif

	__forceinline void TransitionResource(CommandContext* ctx,
										  ShaderResourceCacheD3D12::Resource& res,
										  D3D12_DESCRIPTOR_RANGE_TYPE rangeType)
	{
		if (!res.pObject)
			return;

		const D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_SHADER_RESOURCE = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		switch (res.Type)
		{
		case CachedResourceType::CBV:
			VERIFY_EXPR(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV, "Unexpected descriptor range type");
			if (!res.pObject->CheckAllStates(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
				ctx->TransitionResource(res.pObject.get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			break;
		case CachedResourceType::BufSRV:
			VERIFY_EXPR(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
			if (!res.pObject->CheckAllStates(D3D12_RESOURCE_STATE_SHADER_RESOURCE))
				ctx->TransitionResource(res.pObject.get(), D3D12_RESOURCE_STATE_SHADER_RESOURCE);
			break;
		case CachedResourceType::BufUAV:
			VERIFY_EXPR(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
			if (!res.pObject->CheckAllStates(D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
				ctx->TransitionResource(res.pObject.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			break;
		case CachedResourceType::TexSRV:
			VERIFY_EXPR(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
			if (!res.pObject->CheckAllStates(D3D12_RESOURCE_STATE_SHADER_RESOURCE))
				ctx->TransitionResource(res.pObject.get(), D3D12_RESOURCE_STATE_SHADER_RESOURCE);
			break;
		case CachedResourceType::TexUAV:
			VERIFY_EXPR(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
			if (!res.pObject->CheckAllStates(D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
				ctx->TransitionResource(res.pObject.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			break;
		case CachedResourceType::Sampler:
			VERIFY_EXPR(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "Unexpected descriptor range type");
			break;
		default:
			// Resource not bound
			VERIFY_EXPR(res.Type == CachedResourceType::Unknown, "Unexpected resource type");
			VERIFY_EXPR(res.pObject == nullptr && res.CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
		}
	}

	template<class TOperation>
	__forceinline void ProcessCachedTableResources(uint32 rootInd,
		const D3D12_ROOT_PARAMETER& d3d12Param,
		ShaderResourceCacheD3D12& resourceCache,
		TOperation operation)
	{
		for (UINT r = 0; r < d3d12Param.DescriptorTable.NumDescriptorRanges; ++r)
		{
			const auto& range = d3d12Param.DescriptorTable.pDescriptorRanges[r];
			for (UINT d = 0; d < range.NumDescriptors; ++d)
			{
				auto offsetFromTableStart = range.OffsetInDescriptorsFromTableStart + d;
				auto& res = resourceCache.GetRootTable(rootInd).GetResource(offsetFromTableStart);

				operation(offsetFromTableStart, range, res);
			}
		}
	}

	void RootSignatureD3D12_1::CommitDescriptorHandlesInternal_SM(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache, CommandContext* ctx, bool isCompute, bool transitionResources) const
	{
		VERIFY_EXPR(m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_DYNAMIC] == 0 && m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_DYNAMIC] == 0, "");

		ID3D12DescriptorHeap* descriptorHeaps[] =
		{
			device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
			device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		};

		if (descriptorHeaps)
			ctx->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		m_RootParams.ProcessRootTables(
			[&](uint32 RootInd, const RootParameter& RootTable, const D3D12_ROOT_PARAMETER& D3D12Param, bool IsResourceTable)
			{
				VERIFY_EXPR(RootTable.GetShaderVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC, "Unexpected dynamic resource");

				D3D12_GPU_DESCRIPTOR_HANDLE RootTableGPUDescriptorHandle = IsResourceTable ?
					resourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootInd) :
					resourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootInd);
				VERIFY_EXPR(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");

				if (isCompute)
					ctx->GetCmdList()->SetComputeRootDescriptorTable(RootInd, RootTableGPUDescriptorHandle);
				else
					ctx->GetCmdList()->SetGraphicsRootDescriptorTable(RootInd, RootTableGPUDescriptorHandle);

				if (transitionResources)
				{
					ProcessCachedTableResources(RootInd, D3D12Param, resourceCache,
						[&](UINT offsetFromTableStart, const D3D12_DESCRIPTOR_RANGE& range, ShaderResourceCacheD3D12::Resource& res)
						{
							TransitionResource(ctx, res, range.RangeType);
						}
					);
				}
			}
		);
	}

	void RootSignatureD3D12_1::CommitDescriptorHandlesInternal_SMD(RenderDeviceD3D12* device, ShaderResourceCacheD3D12& resourceCache, CommandContext* ctx, bool isCompute, bool transitionResources) const
	{
		auto* pd3d12Device = device->GetD3D12Device();

		uint32 numDynamicCbvSrvUavDescriptors = m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_DYNAMIC];
		uint32 numDynamicSamplerDescriptors = m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_DYNAMIC];
		VERIFY_EXPR(numDynamicCbvSrvUavDescriptors > 0 || numDynamicSamplerDescriptors > 0, "");

		DescriptorHeapAllocation dynamicCbvSrvUavDescriptors, dynamicSamplerDescriptors;
		if (numDynamicCbvSrvUavDescriptors)
			dynamicCbvSrvUavDescriptors = device->AllocateDynamicDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDynamicCbvSrvUavDescriptors);
		if (numDynamicSamplerDescriptors)
			dynamicSamplerDescriptors = device->AllocateDynamicDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, numDynamicSamplerDescriptors);

		ID3D12DescriptorHeap* descriptorHeaps[] =
		{
			device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
			device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		};

		if (numDynamicCbvSrvUavDescriptors)
			VERIFY_EXPR(dynamicCbvSrvUavDescriptors.GetDescriptorHeap() == descriptorHeaps[0], "Inconsistent CbvSrvUav descriptor heaps");
		if (numDynamicSamplerDescriptors)
			VERIFY_EXPR(dynamicSamplerDescriptors.GetDescriptorHeap() == descriptorHeaps[1], "Inconsistent Sampler descriptor heaps");

		if (descriptorHeaps)
			ctx->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		// Offset to the beginning of the current dynamic CBV_SRV_UAV/SAMPLER table from 
		// the start of the allocation
		uint32 dynamicCbvSrvUavTblOffset = 0;
		uint32 dynamicSamplerTblOffset = 0;

		m_RootParams.ProcessRootTables(
			[&](uint32 rootInd, const RootParameter& rootTable, const D3D12_ROOT_PARAMETER& d3d12Param, bool isResourceTable)
			{
				D3D12_GPU_DESCRIPTOR_HANDLE rootTableGPUDescriptorHandle;
				bool isDynamicTable = rootTable.GetShaderVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC;
				if (isDynamicTable)
				{
					if (isResourceTable)
						rootTableGPUDescriptorHandle = dynamicCbvSrvUavDescriptors.GetGpuHandle(dynamicCbvSrvUavTblOffset);
					else
						rootTableGPUDescriptorHandle = dynamicSamplerDescriptors.GetGpuHandle(dynamicSamplerTblOffset);
				}
				else
				{
					rootTableGPUDescriptorHandle = isResourceTable ?
						resourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(rootInd) :
						resourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(rootInd);
					VERIFY_EXPR(rootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");
				}

				if (isCompute)
					ctx->GetCmdList()->SetComputeRootDescriptorTable(rootInd, rootTableGPUDescriptorHandle);
				else
					ctx->GetCmdList()->SetGraphicsRootDescriptorTable(rootInd, rootTableGPUDescriptorHandle);

				ProcessCachedTableResources(rootInd, d3d12Param, resourceCache,
					[&](UINT offsetFromTableStart, const D3D12_DESCRIPTOR_RANGE& range, ShaderResourceCacheD3D12::Resource& res)
					{
						if (transitionResources)
						{
							TransitionResource(ctx, res, range.RangeType);
						}

						if (isDynamicTable)
						{
							if (isResourceTable)
							{
								if (res.CPUDescriptorHandle.ptr == 0)
									LOG_ERROR("No valid CbvSrvUav descriptor handle found for root parameter ", rootInd, ", descriptor slot ", offsetFromTableStart);

								VERIFY_EXPR(dynamicCbvSrvUavTblOffset < numDynamicCbvSrvUavDescriptors, "Not enough space in the descriptor heap allocation");

								pd3d12Device->CopyDescriptorsSimple(1, dynamicCbvSrvUavDescriptors.GetCpuHandle(dynamicCbvSrvUavTblOffset), res.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
								++dynamicCbvSrvUavTblOffset;
							}
							else
							{
								if (res.CPUDescriptorHandle.ptr == 0)
									LOG_ERROR("No valid sampler descriptor handle found for root parameter ", rootInd, ", descriptor slot ", offsetFromTableStart);

								VERIFY_EXPR(dynamicSamplerTblOffset < numDynamicSamplerDescriptors, "Not enough space in the descriptor heap allocation");

								pd3d12Device->CopyDescriptorsSimple(1, dynamicSamplerDescriptors.GetCpuHandle(dynamicSamplerTblOffset), res.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
								++dynamicSamplerTblOffset;
							}
						}
					}
				);
			}
		);

		VERIFY_EXPR(dynamicCbvSrvUavTblOffset == numDynamicCbvSrvUavDescriptors, "");
		VERIFY_EXPR(dynamicSamplerTblOffset == numDynamicSamplerDescriptors, "");
	}
}