#pragma once

#include "IRenderEngine.h"
#include "RenderPasses.h"
#include "DeviceContext.h"
#include "Timer.h"
#include "Camera.h"
#include "Window.h"
#include "Mesh.h"
#include "Texture.h"

#include <SwapChain.h>
#include <DynamicUploadBuffer.h>

#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

namespace EduEngine
{
	struct EngineInitInfo
	{
		uint32 ImmediateContextsNum = 0;
		uint32 DeferredContextsNum = 0;
		QueueId* ImmediateContextsQueues;
		QueueMask CommandQueues = QueueId::Direct;
	};

	class RenderEngine : public IRenderEngine
	{
	public:
		RenderEngine();
		virtual ~RenderEngine();

		RenderEngine(const RenderEngine& rhs) = delete;
		RenderEngine& operator=(const RenderEngine& rhs) = delete;

		bool StartUp(const Window& mainWindow);

		void Update(const Timer& timer) override;
		void Render(const Timer& timer) override;

		void PendingResize(UINT w, UINT h);

		void AllocImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle);
		void FreeImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);

		static RenderEngine* GetInstance();

	protected:
		virtual void ChangeInitInfo(EngineInitInfo& info) {};
		virtual void OnStartUp() {};
		virtual void OnUpdate(const Timer& timer) {};
		virtual void OnRender(const Timer& timer) {};

		void PopulateDebugImguiCommand();

		RenderDeviceD3D12* GetDevice() const { return m_Device.get(); }
		Camera* GetCamera() const { return m_Camera.get(); }
		DeviceContext* GetMainContext() const { return m_MainContext.get(); }
		DeviceContext* GetImmediateContext(uint16 idx) const;
		DeviceContext* GetDeferredContext(uint16 idx) const;
		SwapChain* GetSwapChain() const { return m_SwapChain.get(); }
		D3D12_VIEWPORT GetViewport() const { return m_Viewport; }
		D3D12_RECT GetScissorRect() const { return m_ScissorRect; }
		EngineInitInfo GetInitInfo() const { return m_InitInfo; }

	private:
		void InitImGui(const Window& mainWindow);
		void Resize(UINT w, UINT h);

	private:
		static RenderEngine* m_Instance;

		std::unique_ptr<RenderDeviceD3D12> m_Device;

		std::unique_ptr<SwapChain> m_SwapChain;
		std::unique_ptr<Camera> m_Camera;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
		DXGI_ADAPTER_DESC1 m_DeviceDesc;

		DescriptorHeapAllocation m_ImGuiTex;

		static constexpr DirectX::SimpleMath::Rectangle EmptyResize = { -1, -1, -1, -1 };
		DirectX::SimpleMath::Rectangle m_PendingResize = EmptyResize;
	
		std::vector<std::unique_ptr<DeviceContext>> m_ImmediateContexts;
		std::vector<std::unique_ptr<DeviceContext>> m_DeferredContexts;
		std::unique_ptr<DeviceContext> m_MainContext;

		EngineInitInfo m_InitInfo;
	};
}