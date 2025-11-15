#pragma once

namespace EduEngine::EduBinding
{
	template<typename TOnRootView, typename TOnDescriptorTable, typename TOnDescriptor>
	__forceinline void ShaderBinder::ProcessRootParams(TOnRootView OnRootView, TOnDescriptorTable OnDescriptorTable, TOnDescriptor OnDescriptor)
	{
		for (uint16 rootIndex = 0; rootIndex < m_RootViewNum + m_DescriptorTablesNum; rootIndex++)
		{
			CachedRootParam* param = reinterpret_cast<CachedRootParam*>(m_Buffer) + rootIndex;

			if (param->IsRootView())
			{
				OnRootView(rootIndex, param);
			}
			else
			{
				OnDescriptorTable(rootIndex, param);

				for (uint16 i = 0; i < param->DescriptorTable.DescriptorsNum; i++)
				{
					CachedDescriptor& descriptor = param->DescriptorTable.pDescriptors[i];
					OnDescriptor(rootIndex, param, &descriptor, i);
				}
			}
		}
	}

	template<typename TOnRootView, typename TOnDescriptor>
	__forceinline void ShaderBinder::ProcessRootParams(SHADER_RESOURCE_TYPE resType,
		EDU_SHADER_TYPE	shaderType,
		TOnRootView			OnRootView,
		TOnDescriptor		OnDescriptor)
	{
		uint16 resNum = 0;
		CachedRootParam* params = GetRootParams(resType, shaderType, resNum);

		if (resNum == 0)
		{
			LOG_ERROR("ShaderBinder does not contain resources for this shader");
			return;
		}

		for (uint16 i = 0; i < resNum; i++)
		{
			auto& rootParam = params[i];

			if (rootParam.IsRootView())
			{
				if (OnRootView(&rootParam))
					return;
			}
			else
			{
				for (uint16 i = 0; i < rootParam.DescriptorTable.DescriptorsNum; i++)
				{
					CachedDescriptor& descriptor = rootParam.DescriptorTable.pDescriptors[i];
					
					if (OnDescriptor(&rootParam, &descriptor, i))
						return;
				}
			}
		}
	}
}