#pragma once
#include "framework.h"
#include "QueueID.h"
#include "ShaderD3D12.h"
#include "RootSignature.h"
#include "ShaderBinder.h"

namespace EduEngine::EduBinding
{
	class EDUBINDING_API PipelineStateBase
	{
	public:
		PipelineStateBase(QueueID queueId, bool isCompute);
		virtual ~PipelineStateBase();

		void Build(RenderDeviceD3D12* pDevice);
		void CommitAll(DeviceContext* context);

		void SetName(const wchar_t* name) const { m_PSO->SetName(name); }

		IShaderBinder* GetShaderBinder() const { return m_ShaderBinder.get(); }

#ifdef EDUBINDINGDEBUG
		void DebugPrint();
#endif

	protected:
		void SetShaderBase(const std::shared_ptr<ShaderD3D12>& shader);

		virtual void BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) = 0;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;

		std::shared_ptr<ShaderD3D12> m_Shaders[EDU_SHADER_TYPE_NUM];
		std::shared_ptr<ShaderBinder> m_ShaderBinder;
		RootSignature m_RootSignature;

		QueueID m_QueueId;
		RenderDeviceD3D12* m_Device;
		bool m_IsCompute;
	};
}