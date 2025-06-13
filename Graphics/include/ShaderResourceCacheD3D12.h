#pragma once

#include "framework.h"
#include "Asserts.h"

#include <DescriptorHeapAllocation.h>
#include <ShaderAPI.h>
#include <ResourceD3D12.h>

namespace EduEngine
{
	enum class CachedResourceType : int
	{
		Unknown = -1,
		CBV = 0,
		TexSRV,
		BufSRV,
		TexUAV,
		BufUAV,
		Sampler,
		NumTypes
	};

	class ShaderResourceCacheD3D12
	{
	public:
		ShaderResourceCacheD3D12() {}
		~ShaderResourceCacheD3D12();

		void Initialize(uint32 numTables, uint32 tableSizes[]);
		void SetDescriptorHeapSpace(DescriptorHeapAllocation&& cbcSrvUavHeapSpace, DescriptorHeapAllocation&& samplerHeapSpace);

#ifdef _DEBUG
		void DebugPrint();
#endif

		static const uint32 InvalidDescriptorOffset = static_cast<uint32>(-1);

		struct Resource
		{
			CachedResourceType Type = CachedResourceType::Unknown;
			union Handle
			{
				D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle;
				D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddres; // for constant buffers only
			} ResHandle;
			std::shared_ptr<ResourceD3D12> pObject = nullptr;
		};

		class RootTable
		{
		public:
			RootTable(uint32 numResources, Resource* pResources) :
				m_NumResources(numResources),
				m_pResources(pResources)
			{ }

			inline Resource& GetResource(uint32 offsetFromTableStart)
			{
				VERIFY_EXPR(offsetFromTableStart < m_NumResources, "Root table at index is not large enough to store descriptor");
				return m_pResources[offsetFromTableStart];
			}

			inline uint32 GetNumResources() const { return m_NumResources; }

			uint32 TableStartOffset = InvalidDescriptorOffset;

		private:
			const uint32 m_NumResources = 0;
			Resource* const m_pResources = nullptr;
		};

		inline RootTable& GetRootTable(uint32 rootIndex)
		{
			VERIFY_EXPR(rootIndex < m_NumTables, "");
			return reinterpret_cast<RootTable*>(m_pMemory)[rootIndex];
		}

		inline uint32 GetNumRootTables() const { return m_NumTables; }

		template<D3D12_DESCRIPTOR_HEAP_TYPE HeapType>
		D3D12_GPU_DESCRIPTOR_HANDLE GetShaderVisibleTableGPUDescriptorHandle(uint32 rootParamInd, uint32 offsetFromTableStart = 0)
		{
			auto& rootParam = GetRootTable(rootParamInd);
			VERIFY_EXPR(rootParam.TableStartOffset != InvalidDescriptorOffset, "");
			VERIFY_EXPR(offsetFromTableStart < rootParam.GetNumResources(), "Offset is out of range");

			D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle = { 0 };

			if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			{
				VERIFY_EXPR(!m_SamplerHeapSpace.IsNull(), "");
				gpuDescriptorHandle = m_SamplerHeapSpace.GetGpuHandle(rootParam.TableStartOffset + offsetFromTableStart);
			}
			else if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
			{
				VERIFY_EXPR(!m_CbcSrvUavHeapSpace.IsNull(), "");
				gpuDescriptorHandle = m_CbcSrvUavHeapSpace.GetGpuHandle(rootParam.TableStartOffset + offsetFromTableStart);
			}
			else
			{
				ASSERT_FAILED("Unexpected descriptor heap type");
			}

			return gpuDescriptorHandle;
		}

		template<D3D12_DESCRIPTOR_HEAP_TYPE HeapType>
		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderVisibleTableCPUDescriptorHandle(uint32 rootParamInd, uint32 offsetFromTableStart = 0)
		{
			auto& rootParam = GetRootTable(rootParamInd);

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = { 0 };
			// Descriptor heap allocation is not assigned for dynamic resources or 
			// in a special case when resource cache is used to store static 
			// variable assignments for a shader
			if (rootParam.TableStartOffset != InvalidDescriptorOffset)
			{
				VERIFY_EXPR(offsetFromTableStart < rootParam.GetNumResources(), "Offset is out of range");
				if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
				{
					VERIFY_EXPR(!m_SamplerHeapSpace.IsNull(), "");
					cpuDescriptorHandle = m_SamplerHeapSpace.GetCpuHandle(rootParam.TableStartOffset + offsetFromTableStart);
				}
				else if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
				{
					VERIFY_EXPR(!m_CbcSrvUavHeapSpace.IsNull(), "");
					cpuDescriptorHandle = m_CbcSrvUavHeapSpace.GetCpuHandle(rootParam.TableStartOffset + offsetFromTableStart);
				}
				else
				{
					ASSERT_FAILED("Unexpected descriptor heap type");
				}
			}

			return cpuDescriptorHandle;
		}

	private:
		ShaderResourceCacheD3D12(const ShaderResourceCacheD3D12&) = delete;
		ShaderResourceCacheD3D12(ShaderResourceCacheD3D12&&) = delete;
		ShaderResourceCacheD3D12& operator = (const ShaderResourceCacheD3D12&) = delete;
		ShaderResourceCacheD3D12& operator = (ShaderResourceCacheD3D12&&) = delete;

		DescriptorHeapAllocation m_SamplerHeapSpace;
		DescriptorHeapAllocation m_CbcSrvUavHeapSpace;

		void* m_pMemory = nullptr;
		uint32 m_NumTables = 0;
	};
}