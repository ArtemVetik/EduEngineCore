#include "PipelineStateBase.h"

namespace EduEngine::EduBinding
{
	PipelineStateBase::PipelineStateBase(QueueMask queueMask, bool isCompute) :
		m_Device(nullptr),
		m_QueueMask(queueMask),
		m_IsCompute(isCompute)
	{
		for (uint32 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
			m_Shaders[i] = nullptr;
	}

	PipelineStateBase::~PipelineStateBase()
	{
		if (!m_Device)
			return;

		ReleaseResourceWrapper staleResource;
		staleResource.Set(std::move(m_PSO));

		m_Device->SafeReleaseObject(std::move(staleResource), m_QueueMask);
		m_Device = nullptr;
	}

	void PipelineStateBase::Build(RenderDeviceD3D12* device, D3D12_STATIC_SAMPLER_DESC* staticSamplers, uint32 numStaticSamplers)
	{
		if (m_Device)
		{
			ReleaseResourceWrapper staleResource;
			staleResource.Set(std::move(m_PSO));
			m_Device->SafeReleaseObject(std::move(staleResource), m_QueueMask);
		}

		m_Device = device;

		uint8 shadersNum = 0;
		ShaderD3D12* activeShaders[EDU_SHADER_TYPE_NUM];

		for (uint8 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
		{
			if (m_Shaders[i].get())
			{
				activeShaders[shadersNum++] = m_Shaders[i].get();
			}
		}

		m_RootSignature.Build(device, activeShaders, shadersNum, staticSamplers, numStaticSamplers);

		BuildPSO(device->GetD3D12Device(), m_RootSignature.GetD3D12RootSignature(), m_PSO);
	}

	void PipelineStateBase::CommitPso(DeviceContext* context)
	{
		context->GetCommandCtx()->GetCmdList()->SetPipelineState(m_PSO.Get());

		if (m_IsCompute)
			context->GetCommandCtx()->GetCmdList()->SetComputeRootSignature(m_RootSignature.GetD3D12RootSignature());
		else
			context->GetCommandCtx()->GetCmdList()->SetGraphicsRootSignature(m_RootSignature.GetD3D12RootSignature());
	}

	void PipelineStateBase::CommitBinder(DeviceContext* context, ShaderBinder* shaderBinder)
	{
		shaderBinder->CommitAll(context, m_IsCompute);
	}

	void PipelineStateBase::CommitAll(DeviceContext* context, ShaderBinder* shaderBinder)
	{
		CommitPso(context);
		CommitBinder(context, shaderBinder);
	}

	std::shared_ptr<ShaderBinder> PipelineStateBase::CreateShaderBinder()
	{
		VERIFY_EXPR(m_Device != nullptr, "You must first call PipelineStateBase::Build() before creating IShaderBinder");

		uint8 shadersNum = 0;
		ShaderD3D12* activeShaders[EDU_SHADER_TYPE_NUM];

		for (uint8 i = 0; i < EDU_SHADER_TYPE_NUM; i++)
		{
			if (m_Shaders[i].get())
			{
				activeShaders[shadersNum++] = m_Shaders[i].get();
			}
		}

		auto shaderBinder = std::make_shared<ShaderBinder>(m_Device);
		shaderBinder->Build(activeShaders, shadersNum);

		return shaderBinder;
	}

	void PipelineStateBase::SetShaderBase(const std::shared_ptr<ShaderD3D12>& shader)
	{
		VERIFY_EXPR(shader != nullptr, "");
		m_Shaders[shader->GetType()] = shader;
	}

#ifdef EDUBINDINGDEBUG
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
	}
#endif
}