#pragma once
#include "framework.h"
#include "QueueMask.h"
#include "ShaderD3D12.h"
#include "RootSignature.h"
#include "ShaderBinder.h"

namespace EduEngine::EduBinding
{
	class EDUBINDING_API PipelineStateBase
	{
	public:
		PipelineStateBase(QueueMask queueMask, bool isCompute);
		virtual ~PipelineStateBase();

		void Build(RenderDeviceD3D12* pDevice, D3D12_STATIC_SAMPLER_DESC* staticSamplers = nullptr, uint32 numStaticSamplers = 0);
		void CommitAll(DeviceContext* context, ShaderBinder* shaderBinder);

		std::shared_ptr<ShaderBinder> CreateShaderBinder();

		void SetName(const wchar_t* name) const { m_PSO->SetName(name); }

#ifdef EDUBINDINGDEBUG
		void DebugPrint();
#endif

	protected:
		void SetShaderBase(const std::shared_ptr<ShaderD3D12>& shader);

		virtual void BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) = 0;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;

		std::shared_ptr<ShaderD3D12> m_Shaders[EDU_SHADER_TYPE_NUM];
		RootSignature m_RootSignature;

		QueueMask m_QueueMask;
		RenderDeviceD3D12* m_Device;
		bool m_IsCompute;
	};
}