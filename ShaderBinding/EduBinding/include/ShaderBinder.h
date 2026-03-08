#pragma once
#include "framework.h"
#include "ShaderD3D12.h"

#include <DeviceContext.h>
#include <DynamicUploadBuffer.h>
#include <ResourceViewD3D12.h>

namespace EduEngine::EduBinding
{
	enum class CachedDescriptorType
	{
		SRV, UAV
	};

	struct CachedDescriptor
	{
		CachedDescriptor() {}

		const char* Name;
		union
		{
			std::shared_ptr<ResourceViewD3D12> Mutable;
			std::shared_ptr<DynamicUploadBuffer> Dynamic;
		};
		uint32 Count;
		CachedDescriptorType Type;
	};

	struct CachedRootView
	{
		CachedRootView() {}

		const char* Name;
		union
		{
			std::shared_ptr<ResourceViewD3D12> Mutable;
			std::shared_ptr<DynamicUploadBuffer> Dynamic;
		};
	};

	struct CachedDescriptorTable
	{
		uint16 DescriptorsNum = 0;
		uint16 GPUHeapOffset = 0;
		CachedDescriptor* pDescriptors = nullptr;
	};

	struct CachedRootParam
	{
		CachedRootParam(bool dynamic) :
			m_IsRootView(true),
			m_IsDynamic(dynamic)
		{
			new (&RootView) CachedRootView();

			if (dynamic)
				new (&RootView.Dynamic) std::shared_ptr<DynamicUploadBuffer>();
			else
				new (&RootView.Mutable) std::shared_ptr<ResourceViewD3D12>();
		}

		CachedRootParam(bool dynamic, uint16 descriptorsNum, CachedDescriptor* descriptors) :
			m_IsRootView(false),
			m_IsDynamic(dynamic)
		{
			new (&DescriptorTable) CachedDescriptorTable();
			DescriptorTable.DescriptorsNum = descriptorsNum;
			DescriptorTable.pDescriptors = descriptors;

			for (uint16 i = 0; i < descriptorsNum; i++)
			{
				if (dynamic)
					new (&descriptors[i].Dynamic) std::shared_ptr<DynamicUploadBuffer>();
				else
					new (&descriptors[i].Mutable) std::shared_ptr<ResourceViewD3D12>();
			}
		}

		~CachedRootParam()
		{
			if (m_IsRootView)
			{
				if (m_IsDynamic)
					RootView.Dynamic.~shared_ptr();
				else
					RootView.Mutable.~shared_ptr();
			}
			else
			{
				for (uint16 i = 0; i < DescriptorTable.DescriptorsNum; i++)
				{
					if (m_IsDynamic)
						DescriptorTable.pDescriptors[i].Dynamic.~shared_ptr();
					else
						DescriptorTable.pDescriptors[i].Mutable.~shared_ptr();
				}
			}
		}

		bool IsRootView() const { return m_IsRootView; }
		bool IsDynamic() const { return m_IsDynamic; }

		union
		{
			CachedRootView RootView;
			CachedDescriptorTable DescriptorTable;
		};

	private:
		bool m_IsRootView = false;
		bool m_IsDynamic = false;
	};

	class EDUBINDING_API ShaderBinder
	{
	public:
		enum CommitFlags : uint32
		{
			DYNAMIC_ROOT_VIEW = 1u << 1,
			DYNAMIC_TABLE = 1u << 2,
			MUTABLE_ROOT_VIEW = 1u << 3,
			MUTABLE_TABLE = 1u << 4,
		};

	public:
		ShaderBinder(RenderDeviceD3D12* device);
		~ShaderBinder();

		void Build(ShaderD3D12** shaders, uint8 shadersNum);

		void BindResource(EDU_SHADER_TYPE					 shaderType,
						  const char*						 name,
						  std::shared_ptr<ResourceViewD3D12> resource,
						  uint32							 srcDescriptorOffset = 0,
						  uint32							 dstDescriptorOffset = 0);
		void BindDynamicResource(EDU_SHADER_TYPE shaderType, const char* name, std::shared_ptr<DynamicUploadBuffer> resource);
		void DryMutableResources();

		void CommitAll(DeviceContext* context, bool isCompute, uint32 flags = static_cast<uint32>(~0u));

#ifdef EDUBINDINGDEBUG
		void DebugPrint();
#endif

	private:
		template<typename TOnRootView, typename TOnDescriptorTable, typename TOnDescriptor>
		void ProcessRootParams(TOnRootView OnRootView, TOnDescriptorTable OnDescriptorTable, TOnDescriptor OnDescriptor);

		template<typename TOnRootView, typename TOnDescriptor>
		void ProcessRootParams(SHADER_RESOURCE_TYPE resType,
							   EDU_SHADER_TYPE	shaderType,
							   TOnRootView			OnRootView,
							   TOnDescriptor		OnDescriptor
		);

		CachedRootParam* GetRootParams(SHADER_RESOURCE_TYPE resType, EDU_SHADER_TYPE shaderType, uint16& outNum) const;
		CachedDescriptor* GetDescriptor(uint16 index) const;

		uint8 m_ResOffsets[SHADER_RESOURCE_TYPE_NUM][EDU_SHADER_TYPE_NUM];
		uint8 m_ResCounts[SHADER_RESOURCE_TYPE_NUM][EDU_SHADER_TYPE_NUM];

		uint16 m_RootViewNum;
		uint16 m_DescriptorTablesNum;
		uint16 m_DescriptorsNum;

		DescriptorHeapAllocation m_MutableHeapSpace;
		void* m_Buffer;

		RenderDeviceD3D12* m_Device;
	};
}

#include "ShaderBinder.hpp"