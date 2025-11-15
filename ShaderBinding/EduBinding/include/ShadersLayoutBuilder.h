#pragma once
#include "ShaderD3D12.h"

namespace EduEngine::EduBinding
{
	template<typename TOnCounted,
			 typename TOnProcessCB,
			 typename TOnProcessTexSRV, 
			 typename TOnProcessBuffSRV, 
			 typename TOnProcessTexUAV,
			 typename TOnProcessBuffUAV,
			 typename TOnShaderStart,
			 typename TOnShaderEnd>
	__forceinline void ProcessShadersLayout(ShaderD3D12**   shaders,
											uint8			  shadersNum,
											TOnCounted		  OnCounted,
											TOnShaderStart	  OnShaderStart,
											TOnProcessCB	  OnProcessCB,
											TOnProcessTexSRV  OnProcessTexSRV,
											TOnProcessBuffSRV OnProcessBuffSRV,
											TOnProcessTexUAV  OnProcessTexUAV,
											TOnProcessBuffUAV OnProcessBuffUAV,
											TOnShaderEnd	  OnShaderEnd)
	{
		uint8 numRootViews = 0;
		uint8 numDescriptorTables = 0;
		uint8 numDescriptors = 0;

		for (short st = 0; st < SHADER_RESOURCE_TYPE_NUM; st++)
		{
			SHADER_RESOURCE_TYPE t = (SHADER_RESOURCE_TYPE)st;

			for (uint8 sn = 0; sn < shadersNum; sn++)
			{
				ShaderD3D12* shader = shaders[sn];
				ShaderResources* resources = shader->GetResources();

				numRootViews += resources->GetCBNum(t);

				uint8 descriptors = resources->GetTexSRVNum(t) +
					resources->GetBuffSRVNum(t) +
					resources->GetTexUAVNum(t) +
					resources->GetBuffUAVNum(t);

				numDescriptors += descriptors;
				numDescriptorTables += (descriptors != 0);
			}
		}

		OnCounted(numRootViews, numDescriptorTables, numDescriptors);

		for (short st = 0; st < SHADER_RESOURCE_TYPE_NUM; st++)
		{
			SHADER_RESOURCE_TYPE t = (SHADER_RESOURCE_TYPE)st;

			for (uint8 sn = 0; sn < shadersNum; sn++)
			{
				ShaderD3D12* shader = shaders[sn];
				ShaderResources* resources = shader->GetResources();

				uint8 cbNum = 0;
				uint8 descriptorsNum = 0;

				OnShaderStart(t, shader->GetType());

				for (size_t rn = 0; rn < resources->GetCBNum(t); rn++)
				{
					OnProcessCB(resources->GetCB(t, rn));
					cbNum++;
				}

				for (size_t rn = 0; rn < resources->GetTexSRVNum(t); rn++)
				{
					OnProcessTexSRV(resources->GetTexSRV(t, rn));
					descriptorsNum++;
				}

				for (size_t rn = 0; rn < resources->GetBuffSRVNum(t); rn++)
				{
					OnProcessBuffSRV(resources->GetBuffSRV(t, rn));
					descriptorsNum++;
				}

				for (size_t rn = 0; rn < resources->GetTexUAVNum(t); rn++)
				{
					OnProcessTexUAV(resources->GetTexUAV(t, rn));
					descriptorsNum++;
				}

				for (size_t rn = 0; rn < resources->GetBuffUAVNum(t); rn++)
				{
					OnProcessBuffUAV(resources->GetBuffUAV(t, rn));
					descriptorsNum++;
				}

				OnShaderEnd(cbNum, descriptorsNum);
			}
		}
	}
}