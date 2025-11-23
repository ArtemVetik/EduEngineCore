#include "PipelineStateBase.h"

namespace EduEngine::EduBinding
{
	PipelineStateBase::PipelineStateBase(QueueID queueId, bool isCompute) :
		m_Device(nullptr),
		m_QueueId(queueId),
		m_IsCompute(isCompute)
	{
		for (uint32 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
			m_Shaders[i] = nullptr;
	}

	PipelineStateBase::~PipelineStateBase()
	{
		if (!m_Device)
			return;

		ReleaseResourceWrapper staleResource = {};
		staleResource.AddPageable(std::move(m_PSO));

		m_Device->SafeReleaseObject(m_QueueId, std::move(staleResource));
	}

	void PipelineStateBase::Build(RenderDeviceD3D12* device)
	{
		uint8 shadersNum = 0;
		ShaderD3D12* activeShaders[EDU_SHADER_TYPE_NUM];

		for (uint8 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
		{
			if (m_Shaders[i].get())
			{
				activeShaders[shadersNum++] = m_Shaders[i].get();
			}
		}

		m_RootSignature.Build(device, activeShaders, shadersNum);

		m_ShaderBinder = std::make_shared<ShaderBinder>(device);
		m_ShaderBinder->Build(activeShaders, shadersNum);

		BuildPSO(device->GetD3D12Device(), m_RootSignature.GetD3D12RootSignature(), m_PSO);
	}

	void PipelineStateBase::CommitAll(DeviceContext* context)
	{
		context->GetCommandCtx()->GetCmdList()->SetPipelineState(m_PSO.Get());

		if (m_IsCompute)
			context->GetCommandCtx()->GetCmdList()->SetComputeRootSignature(m_RootSignature.GetD3D12RootSignature());
		else
			context->GetCommandCtx()->GetCmdList()->SetGraphicsRootSignature(m_RootSignature.GetD3D12RootSignature());

		m_ShaderBinder->CommitAll(context, m_IsCompute);
	}

	void PipelineStateBase::SetShaderBase(const std::shared_ptr<ShaderD3D12>& shader)
	{
		VERIFY_EXPR(shader != nullptr, "");
		VERIFY_EXPR(m_Shaders[shader->GetType()] == nullptr, "");
		m_Shaders[shader->GetType()] = shader;
	}

#ifdef _DEBUG
	void PipelineStateBase::DebugPrint()
	{
		for (uint8 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
		{
			if (m_Shaders[i].get())
			{
				m_Shaders[i]->DebugPrint();
			}
		}

		m_RootSignature.DebugPrint();
		m_ShaderBinder->DebugPrint();
	}
#endif
}