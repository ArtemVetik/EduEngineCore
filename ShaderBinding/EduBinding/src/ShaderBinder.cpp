#include "ShaderBinder.h"
#include "ShadersLayoutBuilder.h"

namespace EduEngine::EduBinding
{
	ShaderBinder::ShaderBinder(RenderDeviceD3D12* device) :
		m_Device(device),
		m_Buffer(nullptr),
		m_RootViewNum(0),
		m_DescriptorTablesNum(0),
		m_DescriptorsNum(0)
	{
	}

	ShaderBinder::~ShaderBinder()
	{
		if (m_Buffer)
		{
			CachedRootParam* rootParams = reinterpret_cast<CachedRootParam*>(m_Buffer);

			for (uint16 rootIndex = 0; rootIndex < m_RootViewNum + m_DescriptorTablesNum; rootIndex++)
				rootParams[rootIndex].~CachedRootParam();

			free(m_Buffer);
			m_Buffer = nullptr;
		}
	}

	void ShaderBinder::Build(ShaderD3D12** shaders, uint8 shadersNum)
	{
		uint16 rootIndex = 0;
		uint16 descriptorsOffset = 0;
		uint16 totalMutableDescriptors = 0;
		uint8 prevResOffset = 0;

		SHADER_RESOURCE_TYPE resType;
		EDU_SHADER_TYPE shaderType;

		memset(m_ResOffsets, -1, sizeof(m_ResOffsets));
		memset(m_ResCounts, 0, sizeof(m_ResCounts));

		ProcessShadersLayout(shaders, shadersNum,
			[&](uint8 rootViewsNum, uint8 descriptorTablesNum, uint8 descriptorsNum)
			{
				m_RootViewNum = rootViewsNum;
				m_DescriptorTablesNum = descriptorTablesNum;
				m_DescriptorsNum = descriptorsNum;

				m_Buffer = malloc((rootViewsNum + descriptorTablesNum) * sizeof(CachedRootParam) + descriptorsNum * sizeof(CachedDescriptor));
			},
			[&](SHADER_RESOURCE_TYPE ResType, EDU_SHADER_TYPE ShaderType) // OnShaderStart
			{
				resType = ResType;
				shaderType = ShaderType;
			},
			[&](ShaderResourceInfo& cb)
			{
				auto rootParam = new (reinterpret_cast<CachedRootParam*>(m_Buffer) + rootIndex++) CachedRootParam(resType == SHADER_RESOURCE_TYPE_DYNAMIC);
				rootParam->RootView.Name = cb.GetName();
			},
			[&](ShaderResourceInfo& texSrv)
			{
				auto descriptor = new (GetDescriptor(descriptorsOffset++)) CachedDescriptor();
				descriptor->Name = texSrv.GetName();
				descriptor->Type = CachedDescriptorType::SRV;
			},
			[&](ShaderResourceInfo& buffSrv)
			{
				auto descriptor = new (GetDescriptor(descriptorsOffset++)) CachedDescriptor();
				descriptor->Name = buffSrv.GetName();
				descriptor->Type = CachedDescriptorType::SRV;
			},
			[&](ShaderResourceInfo& texUAV)
			{
				auto descriptor = new (GetDescriptor(descriptorsOffset++)) CachedDescriptor();
				descriptor->Name = texUAV.GetName();
				descriptor->Type = CachedDescriptorType::UAV;
			},
			[&](ShaderResourceInfo& buffUAV)
			{
				auto descriptor = new (GetDescriptor(descriptorsOffset++)) CachedDescriptor();
				descriptor->Name = buffUAV.GetName();
				descriptor->Type = CachedDescriptorType::UAV;
			},
			[&](uint8 cbNum, uint8 descriptorsNum) // OnShaderEnd
			{
				if (cbNum == 0 && descriptorsNum == 0)
					return;

				m_ResCounts[resType][shaderType] = cbNum + (descriptorsNum != 0);
				m_ResOffsets[resType][shaderType] = prevResOffset;

				prevResOffset += m_ResCounts[resType][shaderType];

				if (descriptorsNum != 0)
				{
					auto rootParam = new (reinterpret_cast<CachedRootParam*>(m_Buffer) + rootIndex++)
						CachedRootParam(resType == SHADER_RESOURCE_TYPE_DYNAMIC, descriptorsNum, GetDescriptor(descriptorsOffset - descriptorsNum));

					if (resType != SHADER_RESOURCE_TYPE_DYNAMIC)
					{
						rootParam->DescriptorTable.GPUHeapOffset = totalMutableDescriptors;
						totalMutableDescriptors += descriptorsNum;
					}
					else
					{
						rootParam->DescriptorTable.GPUHeapOffset = -1;
					}
				}
			}
		);

		VERIFY_EXPR(m_RootViewNum + m_DescriptorTablesNum == rootIndex, "Incorrect ShaderBinder build: root parameter count mismatch");
		VERIFY_EXPR(descriptorsOffset == m_DescriptorsNum, "Incorrect ShaderBinder build: descriptors count mismatch");

		if (totalMutableDescriptors != 0)
			m_MutableHeapSpace = std::move(m_Device->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, totalMutableDescriptors));
	}

	void ShaderBinder::BindResource(EDU_SHADER_TYPE shaderType, const char* name, std::shared_ptr<ResourceViewD3D12> resource, uint32 descriptorOffset)
	{
		ProcessRootParams(SHADER_RESOURCE_TYPE_MUTABLE, shaderType,
			[&](CachedRootParam* param) // OnRootView
			{
				if (strcmp(param->RootView.Name, name) == 0)
				{
					VERIFY_EXPR(!param->IsDynamic(), "GPU Resource ", param->RootView.Name, " (Root CBV) must be mutable");
					param->RootView.Mutable = resource;
					return true;
				}

				return false;
			},
			[&](CachedRootParam* param, CachedDescriptor* descriptor, uint16 offset) // OnDescriptorTable
			{
				if (strcmp(descriptor->Name, name) == 0)
				{
					VERIFY_EXPR(!param->IsDynamic(), "GPU Resource ", param->RootView.Name, " (Descriptor) must be mutable");
					descriptor->Mutable = resource;

					auto srcHandle = descriptor->Type == CachedDescriptorType::SRV ?
						descriptor->Mutable->GetSRVView()->GetCpuHandle(descriptorOffset) :
						descriptor->Mutable->GetUAVView()->GetCpuHandle(descriptorOffset);

					auto dstHandle = m_MutableHeapSpace.GetCpuHandle(param->DescriptorTable.GPUHeapOffset + offset);

					m_Device->GetD3D12Device()->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					return true;
				}

				return false;
			}
		);
	}

	void ShaderBinder::BindDynamicResource(EDU_SHADER_TYPE shaderType, const char* name, std::shared_ptr<DynamicUploadBuffer> resource)
	{
		ProcessRootParams(SHADER_RESOURCE_TYPE_DYNAMIC, shaderType,
			[&](CachedRootParam* param) // OnRootView
			{
				if (strcmp(param->RootView.Name, name) == 0)
				{
					VERIFY_EXPR(param->IsDynamic(), "GPU Resource ", param->RootView.Name, " (Root CBV) must be dynamic");
					param->RootView.Dynamic = resource;
					return true;
				}

				return false;
			},
			[&](CachedRootParam* param, CachedDescriptor* descriptor, uint16 offset) // OnDescriptor
			{
				if (strcmp(descriptor->Name, name) == 0)
				{
					VERIFY_EXPR(param->IsDynamic(), "GPU Resource ", param->RootView.Name, " (Descriptor) must be dynamic");
					descriptor->Dynamic = resource;
					return true;
				}

				return false;
			}
		);
	}

	void ShaderBinder::DryMutableResources()
	{
		if (m_MutableHeapSpace.IsNull())
			return;

		m_MutableHeapSpace = std::move(m_Device->AllocateGPUDescriptor(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			m_MutableHeapSpace.GetNumHandles())
		);
	}

	void ShaderBinder::CommitAll(DeviceContext* context, bool isCompute)
	{
		auto cmdList = context->GetCommandCtx()->GetCmdList();

		ProcessRootParams(
			[&](uint16 rootIndex, CachedRootParam* param) // OnRootView
			{
				if (param->IsDynamic())
				{
					VERIFY_EXPR(param->RootView.Dynamic.get(), "Dynamic GPU resource (", param->RootView.Name, ") for Root CBV is not set");
					if (isCompute)
						cmdList->SetComputeRootConstantBufferView(rootIndex, param->RootView.Dynamic->GetHeapAllocation(context).GetGpuAddress());
					else
						cmdList->SetGraphicsRootConstantBufferView(rootIndex, param->RootView.Dynamic->GetHeapAllocation(context).GetGpuAddress());
				}
				else
				{
					VERIFY_EXPR(param->RootView.Mutable.get(), "Mutable GPU resource (", param->RootView.Name, ") for Root CBV is not set");
					if (isCompute)
						cmdList->SetComputeRootConstantBufferView(rootIndex, param->RootView.Mutable->GetD3D12Resource()->GetGPUVirtualAddress());
					else
						cmdList->SetGraphicsRootConstantBufferView(rootIndex, param->RootView.Mutable->GetD3D12Resource()->GetGPUVirtualAddress());
				}
			},
			[&](uint16 rootIndex, CachedRootParam* param) // OnDescriptorTable
			{
				if (!param->IsDynamic())
				{
					if (isCompute)
						cmdList->SetComputeRootDescriptorTable(rootIndex, m_MutableHeapSpace.GetGpuHandle(param->DescriptorTable.GPUHeapOffset));
					else
						cmdList->SetGraphicsRootDescriptorTable(rootIndex, m_MutableHeapSpace.GetGpuHandle(param->DescriptorTable.GPUHeapOffset));
				}
				else
				{
					auto dynHeapSpace = context->AllocateDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, param->DescriptorTable.DescriptorsNum);

					for (uint16 i = 0; i < param->DescriptorTable.DescriptorsNum; i++)
					{
						CachedDescriptor& descriptor = param->DescriptorTable.pDescriptors[i];

						VERIFY_EXPR(descriptor.Dynamic.get(), "Dynamic GPU resource (", descriptor.Name, ") for descriptor is not set");

						auto srcHandle = descriptor.Type == CachedDescriptorType::SRV ?
							descriptor.Dynamic->GetSRVDescriptorCPUHandle(context) :
							descriptor.Dynamic->GetUAVDescriptorCPUHandle(context);

						auto dstHandle = dynHeapSpace.GetCpuHandle(i);

						m_Device->GetD3D12Device()->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					}

					if (isCompute)
						cmdList->SetComputeRootDescriptorTable(rootIndex, dynHeapSpace.GetGpuHandle());
					else
						cmdList->SetGraphicsRootDescriptorTable(rootIndex, dynHeapSpace.GetGpuHandle());
				}
			},
			[&](uint16 rootIndex, CachedRootParam* param, CachedDescriptor* descriptor, uint16 offset) // OnDescriptors
			{
			}
		);
	}

	CachedRootParam* ShaderBinder::GetRootParams(SHADER_RESOURCE_TYPE resType, EDU_SHADER_TYPE shaderType, uint16& outNum) const
	{
		outNum = m_ResCounts[resType][shaderType];

		if (outNum == 0)
			return nullptr;

		return reinterpret_cast<CachedRootParam*>(m_Buffer) + m_ResOffsets[resType][shaderType];
	}

	CachedDescriptor* ShaderBinder::GetDescriptor(uint16 index) const
	{
		VERIFY_EXPR(index < m_DescriptorsNum, "Descriptor index out of range: index (", index, ") >= m_DescriptorsNum (", m_DescriptorsNum, ")");
		return reinterpret_cast<CachedDescriptor*>(reinterpret_cast<CachedRootParam*>(m_Buffer) + m_RootViewNum + m_DescriptorTablesNum) + index;
	}

#ifdef EDUBINDINGDEBUG
	void ShaderBinder::DebugPrint()
	{
		printf("----------------------------------------\n");
		printf("------------ Shader Binder -------------\n");
		printf("----------------------------------------\n");

		printf("RootViews: %d\nDescriptorTables: %d\nDescriptors: %d\n", m_RootViewNum, m_DescriptorTablesNum, m_DescriptorsNum);

		ProcessRootParams(
			[&](uint16 rootIndex, CachedRootParam* param) // OnRootView
			{
				printf("[%u] ROOT VIEW: %s\tDynamic: %d\t", rootIndex, param->RootView.Name, param->IsDynamic());
				printf("Resource: %p\n", ((param->IsDynamic()) ?
					(void*)param->RootView.Dynamic.get() :
					(void*)param->RootView.Mutable.get()));
			},
			[&](uint16 rootIndex, CachedRootParam* param)
			{
				printf("[%u] DescriptorTable\t Size: %u\n", rootIndex, param->DescriptorTable.DescriptorsNum);
			},
			[&](uint16 rootIndex, CachedRootParam* param, CachedDescriptor* descriptor, uint16 offset) // OnDescriptorTable
			{
				printf("\t%s\tDynamic: %d\t",
					descriptor->Name, param->IsDynamic());
				printf("Resource: %p\n", ((param->IsDynamic()) ?
					(void*)descriptor->Dynamic.get() :
					(void*)descriptor->Mutable.get()));
			});
	}
#endif
}