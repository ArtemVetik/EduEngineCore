#pragma once
#include <RenderEngine.h>
#include <PipelineState.h>

namespace EduEngine
{
	class MultithreadingDemo : public RenderEngine
	{
	public:
		void ChangeInitInfo(EngineInitInfo& info) override;
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	protected:
		static void ThreadWorker(MultithreadingDemo* pThis, const Timer& timer, uint64 contextId);

	private:
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
		std::shared_ptr<IndexBufferD3D12> m_CubeIB;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuff;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuff;

		EduEngine::EduBinding::PipelineState m_PSO;

		std::vector<std::thread> m_Threads;
		std::vector<CommandContext*> m_Contexts;
	};
}