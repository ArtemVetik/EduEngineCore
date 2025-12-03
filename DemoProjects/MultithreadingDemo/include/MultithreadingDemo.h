#pragma once
#include <RenderEngine.h>
#include <PipelineState.h>

namespace EduEngine
{
	class MultithreadingDemo : public RenderEngine
	{
	public:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	protected:
		uint16 GetNumDeferredContexts() const override { return 1; }

	private:
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
		std::shared_ptr<IndexBufferD3D12> m_CubeIB;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuff;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuff;

		EduEngine::EduBinding::PipelineState m_PSO;
	};
}