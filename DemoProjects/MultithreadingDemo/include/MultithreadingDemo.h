#pragma once
#include <RenderEngine.h>
#include <PipelineState.h>

namespace EduEngine
{
	class MultithreadingDemo : public RenderEngine
	{
	public:
		~MultithreadingDemo();

		void ChangeInitInfo(EngineInitInfo& info) override;
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	protected:
		static void ThreadWorker(MultithreadingDemo* pThis, uint64 contextId);

	private:
		static constexpr uint32 TextureCount = 5;

		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
		std::shared_ptr<IndexBufferD3D12> m_CubeIB;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuff;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuff;

		EduEngine::EduBinding::PipelineState m_PSO;
		std::shared_ptr<EduEngine::EduBinding::ShaderBinder> m_Binder[TextureCount];

		const Timer* m_Timer;
		DirectX::XMUINT3 m_GridSize;
		uint32 m_ActiveThreads;

		std::vector<std::thread> m_Threads;
		HANDLE m_MainSemaphore;
		HANDLE m_WorkerSemaphore;

		std::vector<CommandContext*> m_Contexts;
		D3D12_COMMAND_LIST_TYPE ContextTypes[RenderDeviceD3D12::MaxDeviceContexts];
		bool m_ExitApp = false;
	};
}