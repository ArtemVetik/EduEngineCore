#pragma once
#include "framework.h"
#include "QueueID.h"
#include "RenderDeviceD3D12.h"
#include "RootSignatureD3D12.h"
#include "ShaderD3D12.h"

namespace EduEngine
{
	class GRAPHICS_API PipelineStateD3D12Base
	{
	public:
		PipelineStateD3D12Base(QueueID queueId);
		virtual ~PipelineStateD3D12Base();

		void Build(RenderDeviceD3D12* pDevice);

		void BindCB(EDU_SHADER_TYPE shaderType, const char* varName, DynamicAllocation allocation);
		void BindCB(EDU_SHADER_TYPE shaderType, const char* varName, std::shared_ptr<ResourceViewD3D12> resource);
		void BindResource(EDU_SHADER_TYPE shaderType, const char* varName, std::shared_ptr<ResourceViewD3D12> resource);
		void CommitAll(bool transitionResources = false);

		void SetName(const wchar_t* name) { m_PSO->SetName(name); }

		ID3D12PipelineState* GetD3D12PipelineState() const { return m_PSO.Get(); }

#ifdef _DEBUG
		void DebugPrint();
#endif

	protected:
		void SetShaderBase(const std::shared_ptr<ShaderD3D12>& shader);

		virtual void BuildPSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) = 0;

	protected:
		RenderDeviceD3D12* m_Device;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;
		std::shared_ptr<ShaderD3D12> m_Shaders[EDU_SHADER_TYPE_NUM_TYPES];
		std::shared_ptr<ShaderResourceLayoutD3D12> m_ShaderLayouts[EDU_SHADER_TYPE_NUM_TYPES];
		ShaderResourceCacheD3D12 m_ShaderResourceCache;
		RootSignatureD3D12_1 m_RootSignature;
		
		QueueID m_QueueId;
	};
}