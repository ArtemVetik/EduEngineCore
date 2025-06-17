#pragma once
#include "framework.h"
#include "RenderDeviceD3D12.h"
#include "ResourceViewD3D12.h"

#include <ShaderResourcesD3D12.h>
#include <ShaderResourceCacheD3D12.h>
#include <DynamicUploadBuffer.h>

namespace EduEngine
{
	class ShaderResourceLayoutD3D12
	{
	public:
		ShaderResourceLayoutD3D12() {}
		~ShaderResourceLayoutD3D12();

		void Initialize(ID3D12Device* pd3d12Device,
						const std::shared_ptr<const ShaderResourcesD3D12>& pSrcResources,
						const SHADER_VARIABLE_TYPE* varTypes,
						uint32 numAllowedTypes,
						ShaderResourceCacheD3D12* pResourceCache,
						class RootSignatureD3D12_1* pRootSig,
						bool isStatic);

		struct SRV_CBV_UAV;
		ShaderResourceLayoutD3D12::SRV_CBV_UAV& GetVariable(const char* name);

		void CopyStaticResourceDesriptorHandles(const ShaderResourceLayoutD3D12* srcLayout);

#ifdef _DEBUG
		void DebugPrint();
#endif

		struct SRV_CBV_UAV
		{
			static constexpr uint32 InvalidRootIndex = static_cast<uint32>(-1);
			static constexpr uint32 InvalidOffset = static_cast<uint32>(-1);
			static constexpr uint16 InvalidSamplerId = static_cast<uint16>(-1);

			const ShaderResourceAttribs& Attribs;
			const CachedResourceType ResType;
			const uint32 RootIndex;
			const uint32 OffsetFromTableStart;
			const uint16 SamplerId;

			SRV_CBV_UAV(ShaderResourceLayoutD3D12&	 parentLayout,
						const ShaderResourceAttribs& attribs,
						CachedResourceType           resType,
						uint32                       rootIndex,
						uint32                       offsetFromTableStart,
						uint32                       samplerId) :
				m_ParentLayout(parentLayout),
				Attribs(attribs),
				ResType(resType),
				RootIndex(rootIndex),
				OffsetFromTableStart(offsetFromTableStart),
				SamplerId(static_cast<uint16>(samplerId))
			{
				VERIFY_EXPR(RootIndex != InvalidRootIndex, "Root index must be valid");
				VERIFY_EXPR(OffsetFromTableStart != InvalidOffset, "Offset must be valid");
			}

			void BindResource(std::shared_ptr<ResourceViewD3D12> resourceView);
			void BindDynamicResource(DynamicUploadBuffer* dynamicResource);

		private:
			void BindResource_Internal(std::shared_ptr<ResourceViewD3D12> resourceView, DynamicUploadBuffer* dynamicResource);

			ShaderResourceLayoutD3D12& m_ParentLayout;
		};

		struct Sampler
		{
			static const uint32 InvalidRootIndex = static_cast<uint32>(-1);
			static const uint32 InvalidOffset = static_cast<uint32>(-1);

			const ShaderResourceAttribs& Attribs;
			const uint32 RootIndex;
			const uint32 OffsetFromTableStart;

			Sampler(ShaderResourceLayoutD3D12&	 parentResLayout,
					const ShaderResourceAttribs& attribs,
					uint32                       rootIndex,
					uint32                       offsetFromTableStart) :
				m_ParentResLayout(parentResLayout),
				Attribs(attribs),
				RootIndex(rootIndex),
				OffsetFromTableStart(offsetFromTableStart)
			{
				VERIFY_EXPR(RootIndex != InvalidRootIndex, "Root index must be valid");
				VERIFY_EXPR(OffsetFromTableStart != InvalidOffset, "Offset must be valid");
			}

			void BindSampler(std::shared_ptr<ResourceViewD3D12> pTexViewD3D12, D3D12_CPU_DESCRIPTOR_HANDLE ShdrVisibleHeapCPUDescriptorHandle);

		private:
			ShaderResourceLayoutD3D12& m_ParentResLayout;
		};

	private:
		void AllocateMemory();
		void FreeMemory();

		uint32 GetTotalSrvCbvUavCount() const;
		uint32 GetTotalSamplerCount() const;

		uint32 GetSrvCbvUavOffset(SHADER_VARIABLE_TYPE varType, uint32 r) const
		{
			VERIFY_EXPR(r < m_NumCbvSrvUav[varType], "");
			static_assert(SHADER_VARIABLE_TYPE_STATIC == 0, "SHADER_VARIABLE_TYPE_STATIC == 0 expected");
			r += (varType > SHADER_VARIABLE_TYPE_STATIC) ? m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_STATIC] : 0;
			static_assert(SHADER_VARIABLE_TYPE_MUTABLE == 1, "SHADER_VARIABLE_TYPE_MUTABLE == 1 expected");
			r += (varType > SHADER_VARIABLE_TYPE_MUTABLE) ? m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_MUTABLE] : 0;
			return r;
		}

		SRV_CBV_UAV& GetSrvCbvUav(SHADER_VARIABLE_TYPE varType, uint32 r)
		{
			VERIFY_EXPR(r < m_NumCbvSrvUav[varType], "");
			auto* cbvSrvUav = reinterpret_cast<SRV_CBV_UAV*>(m_ResourceBuffer);
			return cbvSrvUav[GetSrvCbvUavOffset(varType, r)];
		}

		SRV_CBV_UAV& GetSrvCbvUav(uint32 r)
		{
			VERIFY_EXPR(r < GetTotalSrvCbvUavCount(), "");
			auto* cbvSrvUav = reinterpret_cast<SRV_CBV_UAV*>(m_ResourceBuffer);
			return cbvSrvUav[r];
		}

		uint32 GetSamplerOffset(SHADER_VARIABLE_TYPE varType, uint32 s)const
		{
			VERIFY_EXPR(s < m_NumSamplers[varType], "");
			static_assert(SHADER_VARIABLE_TYPE_STATIC == 0, "SHADER_VARIABLE_TYPE_STATIC == 0 expected");
			s += (varType > SHADER_VARIABLE_TYPE_STATIC) ? m_NumSamplers[SHADER_VARIABLE_TYPE_STATIC] : 0;
			static_assert(SHADER_VARIABLE_TYPE_MUTABLE == 1, "SHADER_VARIABLE_TYPE_MUTABLE == 1 expected");
			s += (varType > SHADER_VARIABLE_TYPE_MUTABLE) ? m_NumSamplers[SHADER_VARIABLE_TYPE_MUTABLE] : 0;
			return s;
		}

		Sampler& GetSampler(SHADER_VARIABLE_TYPE varType, uint32 s)
		{
			VERIFY_EXPR(s < m_NumSamplers[varType], "");
			return m_Samplers[GetSamplerOffset(varType, s)];
		}

		Sampler& GetSampler(uint32 s)
		{
			VERIFY_EXPR(s < GetTotalSamplerCount(), "");
			return m_Samplers[s];
		}

	private:
		uint16 m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_NUM_TYPES] = { 0,0,0 };
		uint16 m_NumSamplers[SHADER_VARIABLE_TYPE_NUM_TYPES] = { 0,0,0 };

		ShaderResourceCacheD3D12* m_pResourceCache;

		void* m_ResourceBuffer = nullptr;
		Sampler* m_Samplers = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Device> m_pd3d12Device;
		// We must use shared_ptr to reference ShaderResources instance, because
		// there may be multiple objects referencing the same set of resources
		std::shared_ptr<const ShaderResourcesD3D12> m_pResources;
	};
}