#include "PipelineStateD3D12Base.h"

namespace EduEngine::DiligentBinding
{
	PipelineStateD3D12Base::PipelineStateD3D12Base(QueueMask queueMask, bool isCompute) :
		m_Device(nullptr),
		m_QueueMask(queueMask),
		m_IsCompute(isCompute)
	{
		for (uint32 i = 0; i < EDU_SHADER_TYPE_NUM_TYPES; i++)
		{
			m_Shaders[i] = nullptr;
			m_ShaderLayouts[i] = nullptr;
		}
	}

	PipelineStateD3D12Base::~PipelineStateD3D12Base()
	{
		if (!m_Device)
			return;

		ReleaseResourceWrapper staleResource(m_QueueMask);
		staleResource.Set(std::move(m_PSO));

		m_Device->SafeReleaseObject(std::move(staleResource));
	}

	void PipelineStateD3D12Base::Build(RenderDeviceD3D12* pDevice)
	{
		VERIFY_EXPR(m_Device == nullptr, "");
		m_Device = pDevice;

		const ShaderD3D12* activeShaders[EDU_SHADER_TYPE_NUM_TYPES];
		uint32 activeShadersNum = 0;

		for (size_t i = 0; i < EDU_SHADER_TYPE_NUM_TYPES; i++)
		{
			if (m_Shaders[i])
				activeShaders[activeShadersNum++] = m_Shaders[i].get();
		}

		m_RootSignature.AllocateStaticSamplers(activeShaders, activeShadersNum);

		for (size_t i = 0; i < EDU_SHADER_TYPE_NUM_TYPES; i++)
		{
			if (!m_Shaders[i])
				continue;

			m_ShaderLayouts[i] = std::make_shared<ShaderResourceLayoutD3D12>();
			m_ShaderLayouts[i]->Initialize(m_Device->GetD3D12Device(), m_Shaders[i]->GetShaderResources(), nullptr, 0, &m_ShaderResourceCache, &m_RootSignature, false);
		}

		m_RootSignature.Build(m_Device, m_ShaderResourceCache);

		for (size_t i = 0; i < EDU_SHADER_TYPE_NUM_TYPES; i++)
		{
			if (!m_Shaders[i])
				continue;

			m_ShaderLayouts[i]->CopyStaticResourceDesriptorHandles(m_Shaders[i]->GetStaticLayout());
		}

		BuildPSO(m_Device->GetD3D12Device(), m_RootSignature.GetD3D12RootSignature(), m_PSO);
	}

	void PipelineStateD3D12Base::BindResource(EDU_SHADER_TYPE shaderType, const char* varName, std::shared_ptr<ResourceViewD3D12> resource)
	{
		VERIFY_EXPR(m_ShaderLayouts[shaderType] != nullptr, "");
		m_ShaderLayouts[shaderType]->GetVariable(varName).BindResource(resource);
	}

	void PipelineStateD3D12Base::BindDynamicResource(EDU_SHADER_TYPE shaderType, const char* varName, std::shared_ptr<DynamicUploadBuffer> resource, DeviceContext* ctx)
	{
		VERIFY_EXPR(m_ShaderLayouts[shaderType] != nullptr, "");
		m_ShaderLayouts[shaderType]->GetVariable(varName).BindDynamicResource(resource, ctx);
	}

	void PipelineStateD3D12Base::CommitAll(DeviceContext* context, bool transitionResources)
	{
		context->GetCommandCtx()->GetCmdList()->SetPipelineState(m_PSO.Get());
		if (m_IsCompute)
			context->GetCommandCtx()->GetCmdList()->SetComputeRootSignature(m_RootSignature.GetD3D12RootSignature());
		else
			context->GetCommandCtx()->GetCmdList()->SetGraphicsRootSignature(m_RootSignature.GetD3D12RootSignature());

		m_RootSignature.CommitRootViews(m_ShaderResourceCache, context, m_IsCompute);
		m_RootSignature.CommitDescriptorHandles(m_Device, m_ShaderResourceCache, context, m_IsCompute, transitionResources);
	}

#ifdef _DEBUG
	void PipelineStateD3D12Base::DebugPrint()
	{
		printf("=======================================================\n");
		printf("================== Pipeline State =====================\n");
		printf("=======================================================\n\n");

		for (size_t i = 0; i < EDU_SHADER_TYPE_NUM_TYPES; i++)
		{
			if (!m_Shaders[i])
				continue;

			m_Shaders[i]->DebugPrint();
		}

		for (size_t i = 0; i < EDU_SHADER_TYPE_NUM_TYPES; i++)
		{
			if (!m_ShaderLayouts[i])
				continue;

			m_ShaderLayouts[i]->DebugPrint();
			printf("\n");
		}

		m_RootSignature.DebugPrint();
		m_ShaderResourceCache.DebugPrint();
		printf("\n\n");
	}
#endif

	void PipelineStateD3D12Base::SetShaderBase(const std::shared_ptr<ShaderD3D12>& shader)
	{
		VERIFY_EXPR(shader != nullptr, "");
		VERIFY_EXPR(m_Shaders[shader->GetShaderType()] == nullptr, "");
		m_Shaders[shader->GetShaderType()] = shader;
	}
}