#include "ShaderResourceLayoutD3D12.h"
#include "RootSignatureD3D12.h"
#include "DebugEnumPrint.h"

namespace EduEngine
{
	D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType(CachedResourceType resType)
	{
		switch (resType)
		{
		case EduEngine::CachedResourceType::CBV:
			return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case EduEngine::CachedResourceType::TexSRV:
		case EduEngine::CachedResourceType::BufSRV:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case EduEngine::CachedResourceType::TexUAV:
		case EduEngine::CachedResourceType::BufUAV:
			return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case EduEngine::CachedResourceType::Sampler:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		default:
			ASSERT_FAILED("Unexpected resource type");
			return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		}
	}

	ShaderResourceLayoutD3D12::~ShaderResourceLayoutD3D12()
	{
		FreeMemory();
	}

	void ShaderResourceLayoutD3D12::Initialize(ID3D12Device* pd3d12Device,
											   const std::shared_ptr<const ShaderResourcesD3D12>& pSrcResources,
											   const SHADER_VARIABLE_TYPE* varTypes,
											   uint32 numAllowedTypes,
											   ShaderResourceCacheD3D12* pResourceCache,
											   RootSignatureD3D12_1* pRootSig,
											   bool isStatic)
	{
		m_pResources = pSrcResources;
		m_pResourceCache = pResourceCache;
		m_pd3d12Device = pd3d12Device;

		VERIFY_EXPR(pResourceCache != nullptr, "");
		VERIFY_EXPR((pRootSig != nullptr) ^ isStatic, "");

		// TODO: verify allowed varTypes

		m_pResources->ProcessResources(
			varTypes,
			numAllowedTypes,
			[&](const ShaderResourceAttribs& cb)
			{
				++m_NumCbvSrvUav[cb.GetVarType()];
			},
			[&](const ShaderResourceAttribs& texSRV)
			{
				auto varType = texSRV.GetVarType();
				++m_NumCbvSrvUav[varType];

				if (texSRV.HasValidSampler())
				{
					auto samplerId = texSRV.GetSamplerId();
					const auto& samplerAttribs = m_pResources->GetSampler(samplerId);
					VERIFY_EXPR(samplerAttribs.GetVarType() == varType, "Texture and sampler variable types are not conistent");
					if (!samplerAttribs.IsStaticSampler())
					{
						++m_NumSamplers[varType];
					}
				}
			},
			[&](const ShaderResourceAttribs& texUAV)
			{
				++m_NumCbvSrvUav[texUAV.GetVarType()];
			},
			[&](const ShaderResourceAttribs& bufSRV)
			{
				++m_NumCbvSrvUav[bufSRV.GetVarType()];
			},
			[&](const ShaderResourceAttribs& bufUAV)
			{
				++m_NumCbvSrvUav[bufUAV.GetVarType()];
			}
		);

		AllocateMemory();

		uint32 currCbvSrvUav[SHADER_VARIABLE_TYPE_NUM_TYPES] = { 0,0,0 };
		uint32 currSampler[SHADER_VARIABLE_TYPE_NUM_TYPES] = { 0,0,0 };
		int maxBindPoint[4] = { 0, 0, 0, 0 };

		auto AddResource = [&](const ShaderResourceAttribs& attribs, CachedResourceType resType, uint32 samplerId = SRV_CBV_UAV::InvalidSamplerId)
			{
				uint32 rootIndex = SRV_CBV_UAV::InvalidRootIndex;
				uint32 offset = SRV_CBV_UAV::InvalidOffset;
				D3D12_DESCRIPTOR_RANGE_TYPE descriptorRangeType = GetDescriptorRangeType(resType);
				if (pRootSig)
				{
					pRootSig->AllocateResourceSlot(m_pResources->GetShaderType(), attribs, descriptorRangeType, rootIndex, offset);
				}
				else
				{
					// If root signature is not provided - use artifial root signature to store
					// static shader resources
					rootIndex = descriptorRangeType;
					offset = attribs.BindPoint;
					// Resources in the static resource cache are indexed by the bind point
					maxBindPoint[rootIndex] = std::max(maxBindPoint[rootIndex], static_cast<int>(offset + attribs.BindCount));
				}
				VERIFY_EXPR(rootIndex != SRV_CBV_UAV::InvalidRootIndex, "Root index must be valid");
				VERIFY_EXPR(offset != SRV_CBV_UAV::InvalidOffset, "Offset must be valid");

				// Static samplers are never copied, and SamplerId == InvalidSamplerId
				::new (&GetSrvCbvUav(attribs.GetVarType(), currCbvSrvUav[attribs.GetVarType()]++)) SRV_CBV_UAV(*this, attribs, resType, rootIndex, offset, samplerId);
			};

		// TODO: verify allowed varTypes

		m_pResources->ProcessResources(
			varTypes,
			numAllowedTypes,
			[&](const ShaderResourceAttribs& cb)
			{
				AddResource(cb, CachedResourceType::CBV);
			},
			[&](const ShaderResourceAttribs& texSRV)
			{
				auto varType = texSRV.GetVarType();

				uint32 samplerId = SRV_CBV_UAV::InvalidSamplerId;
				if (texSRV.IsValidSampler())
				{
					const auto& srcSamplerAttribs = m_pResources->GetSampler(texSRV.GetSamplerId());
					VERIFY_EXPR(srcSamplerAttribs.GetVarType() == varType, "Inconsistent texture and sampler variable types");

					if (srcSamplerAttribs.IsStaticSampler())
					{
						if (pRootSig != nullptr)
							pRootSig->InitStaticSampler(m_pResources->GetShaderType(), texSRV.Name, srcSamplerAttribs);

						// Static samplers are never copied, and SamplerId == InvalidSamplerId
					}
					else
					{
						uint32 samplerRootIndex = Sampler::InvalidRootIndex;
						uint32 samplerOffset = Sampler::InvalidOffset;
						if (pRootSig)
						{
							pRootSig->AllocateResourceSlot(m_pResources->GetShaderType(), srcSamplerAttribs, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, samplerRootIndex, samplerOffset);
						}
						else
						{
							// If root signature is not provided, we are initializing resource cache to store 
							// static shader resources. 
							VERIFY_EXPR(m_pResourceCache != nullptr, "");

							// We use the following artifial root signature:
							// CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
							// SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
							// UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV, and
							// Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
							// Every resource is stored at offset that equals its bind point
							samplerRootIndex = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
							samplerOffset = srcSamplerAttribs.BindPoint;
							// Resources in the static resource cache are indexed by the bind point
							maxBindPoint[samplerRootIndex] = std::max(maxBindPoint[samplerRootIndex], static_cast<int>(samplerOffset + srcSamplerAttribs.BindCount));
						}
						VERIFY_EXPR(samplerRootIndex != Sampler::InvalidRootIndex, "Sampler root index must be valid");
						VERIFY_EXPR(samplerOffset != Sampler::InvalidOffset, "Sampler offset must be valid");

						samplerId = currSampler[varType];
						::new (&GetSampler(varType, currSampler[varType]++)) Sampler(*this, srcSamplerAttribs, samplerRootIndex, samplerOffset);
					}
				}
				AddResource(texSRV, CachedResourceType::TexSRV, samplerId);
			},
			[&](const ShaderResourceAttribs& texUAV)
			{
				AddResource(texUAV, CachedResourceType::TexUAV);
			},
			[&](const ShaderResourceAttribs& bufSRV)
			{
				AddResource(bufSRV, CachedResourceType::BufSRV);
			},
			[&](const ShaderResourceAttribs& bufUAV)
			{
				AddResource(bufUAV, CachedResourceType::BufUAV);
			}
		);

		if (isStatic)
		{
			VERIFY_EXPR(pRootSig == nullptr, "");
			uint32 cachedTblSizes[4] =
			{
				static_cast<uint32>(maxBindPoint[D3D12_DESCRIPTOR_RANGE_TYPE_SRV]),
				static_cast<uint32>(maxBindPoint[D3D12_DESCRIPTOR_RANGE_TYPE_UAV]),
				static_cast<uint32>(maxBindPoint[D3D12_DESCRIPTOR_RANGE_TYPE_CBV]),
				static_cast<uint32>(maxBindPoint[D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER])
			};
			m_pResourceCache->Initialize(_countof(cachedTblSizes), cachedTblSizes);
		}
	}

	ShaderResourceLayoutD3D12::SRV_CBV_UAV& ShaderResourceLayoutD3D12::GetVariable(const char* name)
	{
		uint32 count = GetTotalSrvCbvUavCount();

		for (uint32 i = 0; i < count; i++)
		{
			auto& cbvSrvUav = GetSrvCbvUav(i);

			if (strcmp(cbvSrvUav.Attribs.Name, name) == 0)
				return cbvSrvUav;
		}

		ASSERT_FAILED("Can't find variable: ", name);
	}

	void ShaderResourceLayoutD3D12::CopyStaticResourceDesriptorHandles(const ShaderResourceLayoutD3D12* srcLayout)
	{
		if (!m_pResourceCache)
		{
			LOG_ERROR("Resource layout has no resource cache");
			return;
		}

		if (!srcLayout->m_pResourceCache)
		{
			LOG_ERROR("Dst layout has no resource cache");
			return;
		}

		// Static shader resources are stored as follows:
		// CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		// SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		// UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV, and
		// Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
		// Every resource is stored at offset that equals resource bind point

		for (uint32 r = 0; r < m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_STATIC]; ++r)
		{
			const auto& res = GetSrvCbvUav(SHADER_VARIABLE_TYPE_STATIC, r);
			VERIFY_EXPR(srcLayout->m_pResources->GetShaderType() == m_pResources->GetShaderType(), "Incosistent shader types");
			auto rangeType = GetDescriptorRangeType(res.ResType);

			for (uint32 arrInd = 0; arrInd < res.Attribs.BindCount; ++arrInd)
			{
				auto bindPoint = res.Attribs.BindPoint + arrInd;
				// Static resources are indexed in the resource cache by its bind point
				const auto& srcRes = srcLayout->m_pResourceCache->GetRootTable(rangeType).GetResource(bindPoint);

				//if (!srcRes.pObject) // TODO: Log Error
				//    LOG_ERROR_MESSAGE("No resource assigned to static shader variable \"", res.Attribs.GetPrintName(arrInd), "\" in shader \"", GetShaderName(), "\".");

				auto& dstRes = m_pResourceCache->GetRootTable(res.RootIndex).GetResource(res.OffsetFromTableStart + arrInd);

				if (dstRes.pObject != srcRes.pObject)
				{
					VERIFY_EXPR(dstRes.pObject == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");

					dstRes.pObject = srcRes.pObject;
					dstRes.Type = srcRes.Type;
					dstRes.CPUDescriptorHandle = srcRes.CPUDescriptorHandle;

					auto shdrVisibleHeapCPUDescriptorHandle = m_pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(res.RootIndex, res.OffsetFromTableStart + arrInd);
					VERIFY_EXPR(shdrVisibleHeapCPUDescriptorHandle.ptr != 0 || dstRes.Type == CachedResourceType::CBV, "");
					if (shdrVisibleHeapCPUDescriptorHandle.ptr != 0)
					{
						m_pd3d12Device->CopyDescriptorsSimple(1, shdrVisibleHeapCPUDescriptorHandle, srcRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					}
				}
				else
				{
					VERIFY_EXPR(dstRes.pObject == srcRes.pObject, "");
					VERIFY_EXPR(dstRes.Type == srcRes.Type, "");
					VERIFY_EXPR(dstRes.CPUDescriptorHandle.ptr == srcRes.CPUDescriptorHandle.ptr, "");
				}
			}

			if (res.SamplerId != SRV_CBV_UAV::InvalidSamplerId)
			{
				auto& samInfo = GetSampler(res.Attribs.GetVarType(), res.SamplerId);

				VERIFY_EXPR(!samInfo.Attribs.IsStaticSampler(), "Static samplers should never be assigned space in the cache");
				VERIFY_EXPR(samInfo.Attribs.IsValidBindPoint(), "Sampler bind point must be valid");
				VERIFY_EXPR(samInfo.Attribs.BindCount == res.Attribs.BindCount || samInfo.Attribs.BindCount == 1, "");

				for (uint32 arrInd = 0; arrInd < samInfo.Attribs.BindCount; ++arrInd)
				{
					auto bBindPoint = samInfo.Attribs.BindPoint + arrInd;
					// Static resources are indexed by the bind point in the resource cache
					auto& srcSampler = srcLayout->m_pResourceCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER).GetResource(bBindPoint);
					//if (!srcSampler.pObject)
					//    LOG_ERROR_MESSAGE("No sampler assigned to static shader variable \"", res.Attribs.GetPrintName(arrInd), "\" in shader \"", GetShaderName(), "\".");

					auto& dstSampler = m_pResourceCache->GetRootTable(samInfo.RootIndex).GetResource(samInfo.OffsetFromTableStart + arrInd);

					if (dstSampler.pObject != srcSampler.pObject)
					{
						VERIFY_EXPR(dstSampler.pObject == nullptr, "Static sampler resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");

						dstSampler.pObject = srcSampler.pObject;
						dstSampler.Type = srcSampler.Type;
						dstSampler.CPUDescriptorHandle = srcSampler.CPUDescriptorHandle;

						auto shdrVisibleSamplerHeapCPUDescriptorHandle = m_pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(samInfo.RootIndex, samInfo.OffsetFromTableStart + arrInd);
						VERIFY_EXPR(shdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0, "");
						if (shdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0)
						{
							m_pd3d12Device->CopyDescriptorsSimple(1, shdrVisibleSamplerHeapCPUDescriptorHandle, srcSampler.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
						}
					}
					else
					{
						VERIFY_EXPR(dstSampler.pObject == srcSampler.pObject, "");
						VERIFY_EXPR(dstSampler.Type == srcSampler.Type, "");
						VERIFY_EXPR(dstSampler.CPUDescriptorHandle.ptr == srcSampler.CPUDescriptorHandle.ptr, "");
					}
				}
			}
		}
	}

#ifdef _DEBUG
	void ShaderResourceLayoutD3D12::DebugPrint()
	{
		printf("----------------------------------------------------------\n");
		printf("-------------- ShaderResourceLayout [%s] --------------\n", ShaderTypeStr(m_pResources->GetShaderType()));
		printf("----------------------------------------------------------\n");
		printf("CbvSrvUav count: %u\n", GetTotalSrvCbvUavCount());
		printf("Sampler count: %u\n", GetTotalSamplerCount());

		for (uint32 i = 0; i < GetTotalSrvCbvUavCount(); i++)
		{
			auto& c = GetSrvCbvUav(i);
			printf("%u: Name: %s\tRootInd: %u\tOffsetFromStart %u\tResType: %s\tSamplerId: %u\n", i, c.Attribs.Name, c.RootIndex, c.OffsetFromTableStart, ResTypeStr(c.ResType), c.SamplerId);
		}
		printf("\n");
		for (uint32 i = 0; i < GetTotalSamplerCount(); i++)
		{
			auto& c = GetSampler(i);
			printf("%u: Name: %s\tRootInd: %u\tOffsetFromStart %u\n", GetTotalSrvCbvUavCount() + i, c.Attribs.Name, c.RootIndex, c.OffsetFromTableStart);
		}
	}
#endif

	void ShaderResourceLayoutD3D12::AllocateMemory()
	{
		uint32 totalSrvCbvUav = GetTotalSrvCbvUavCount();
		uint32 totalSamplers = GetTotalSamplerCount();
		size_t memSize = totalSrvCbvUav * sizeof(SRV_CBV_UAV) + totalSamplers * sizeof(Sampler);
		if (memSize == 0)
			return;
		auto* pRawMem = std::malloc(memSize);

		if (!pRawMem)
			ASSERT_FAILED("Failed to allocate memory");

		if (m_ResourceBuffer)
			FreeMemory();

		m_ResourceBuffer = pRawMem;

		if (totalSamplers)
			m_Samplers = reinterpret_cast<Sampler*>(reinterpret_cast<SRV_CBV_UAV*>(pRawMem) + totalSrvCbvUav);
	}

	void ShaderResourceLayoutD3D12::FreeMemory()
	{
		if (!m_ResourceBuffer)
			return;

		auto* cbvSrvUav = reinterpret_cast<SRV_CBV_UAV*>(m_ResourceBuffer);
		for (uint32 r = 0; r < GetTotalSrvCbvUavCount(); ++r)
			cbvSrvUav[r].~SRV_CBV_UAV();

		for (uint32 s = 0; s < GetTotalSamplerCount(); ++s)
			m_Samplers[s].~Sampler();

		std::free(m_ResourceBuffer);
	}

	uint32 ShaderResourceLayoutD3D12::GetTotalSrvCbvUavCount() const
	{
		return m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_STATIC] + m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_MUTABLE] + m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_DYNAMIC];
	}

	uint32 ShaderResourceLayoutD3D12::GetTotalSamplerCount() const
	{
		return m_NumSamplers[SHADER_VARIABLE_TYPE_STATIC] + m_NumSamplers[SHADER_VARIABLE_TYPE_MUTABLE] + m_NumSamplers[SHADER_VARIABLE_TYPE_DYNAMIC];
	}

	void ShaderResourceLayoutD3D12::SRV_CBV_UAV::BindResource(std::shared_ptr<ResourceViewD3D12> resourceView)
	{
		BindResource_Internal(resourceView, nullptr);
	}

	void ShaderResourceLayoutD3D12::SRV_CBV_UAV::BindDynamicResource(std::shared_ptr<DynamicUploadBuffer> dynamicResource)
	{
		BindResource_Internal(nullptr, dynamicResource);
	}

	void ShaderResourceLayoutD3D12::SRV_CBV_UAV::BindResource_Internal(std::shared_ptr<ResourceViewD3D12> resourceView, std::shared_ptr<DynamicUploadBuffer> dynamicResource)
	{
		VERIFY_EXPR(resourceView != nullptr || dynamicResource != nullptr, "");
		
		auto& resourceCache = m_ParentLayout.m_pResourceCache;
		VERIFY_EXPR(resourceCache, "Resource cache is null");
		auto& dstRes = resourceCache->GetRootTable(RootIndex).GetResource(OffsetFromTableStart);
		dstRes.Type = ResType;
		dstRes.pObject = resourceView;
		dstRes.pDynObject = dynamicResource;

		if (ResType == CachedResourceType::CBV)
			return;

		auto shdrVisibleHeapCPUDescriptorHandle = resourceCache->GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootIndex, OffsetFromTableStart);

		switch (dstRes.Type)
		{
		case CachedResourceType::TexSRV:
		case CachedResourceType::BufSRV:
			dstRes.CPUDescriptorHandle = resourceView ?
				dstRes.CPUDescriptorHandle = resourceView->GetSRVView()->GetCpuHandle() :
				dynamicResource->GetSRVDescriptorCPUHandle();
			break;
		case CachedResourceType::TexUAV:
		case CachedResourceType::BufUAV:
			dstRes.CPUDescriptorHandle = resourceView ?
				dstRes.CPUDescriptorHandle = resourceView->GetUAVView()->GetCpuHandle() :
				dynamicResource->GetUAVDescriptorCPUHandle();
			break;
		default:
			ASSERT_FAILED("Invalid resource type");
			break;
		}

		VERIFY_EXPR(dstRes.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 view");

		if (shdrVisibleHeapCPUDescriptorHandle.ptr != 0)
		{
			ID3D12Device* pd3d12Device = m_ParentLayout.m_pd3d12Device.Get();
			pd3d12Device->CopyDescriptorsSimple(1, shdrVisibleHeapCPUDescriptorHandle, dstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		if (SamplerId != InvalidSamplerId)
		{
			VERIFY_EXPR(resourceView != nullptr, "");
			auto& sampler = m_ParentLayout.GetSampler(Attribs.GetVarType(), SamplerId);

			VERIFY_EXPR(!sampler.Attribs.IsStaticSampler(), "Static samplers should never be assigned space in the cache");
			VERIFY_EXPR(Attribs.BindCount == sampler.Attribs.BindCount || sampler.Attribs.BindCount == 1, "");
			auto shdrVisibleSamplerHeapCPUDescriptorHandle = resourceCache->GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(sampler.RootIndex, sampler.OffsetFromTableStart);
			sampler.BindSampler(resourceView, shdrVisibleSamplerHeapCPUDescriptorHandle);
		}
	}

	void ShaderResourceLayoutD3D12::Sampler::BindSampler(std::shared_ptr<ResourceViewD3D12> pTexViewD3D12, D3D12_CPU_DESCRIPTOR_HANDLE ShdrVisibleHeapCPUDescriptorHandle)
	{
		auto& resourceCache = m_ParentResLayout.m_pResourceCache;
		VERIFY_EXPR(resourceCache, "Resource cache is null");
		VERIFY_EXPR(Attribs.IsValidBindPoint(), "Invalid bind point");

		auto& dstSam = resourceCache->GetRootTable(RootIndex).GetResource(OffsetFromTableStart);

		if (pTexViewD3D12)
		{
			auto sampler = pTexViewD3D12->GetSampler();
			VERIFY_EXPR(sampler != nullptr, "");

			dstSam.Type = CachedResourceType::Sampler;
			dstSam.CPUDescriptorHandle = sampler->GetCpuHandle();
			dstSam.pObject = pTexViewD3D12;

			VERIFY_EXPR(dstSam.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 sampler descriptor handle");

			if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
			{
				ID3D12Device* pd3d12Device = m_ParentResLayout.m_pd3d12Device.Get();
				pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, dstSam.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}
		else
		{
			dstSam = ShaderResourceCacheD3D12::Resource();
		}
	}
}