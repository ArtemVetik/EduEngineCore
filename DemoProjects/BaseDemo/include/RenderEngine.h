#pragma once

#include "IRenderEngine.h"
#include "RenderPasses.h"
#include "DeviceContext.h"
#include "Timer.h"
#include "Camera.h"
#include "Window.h"
#include "Mesh.h"
#include "Texture.h"

#include "../../Graphics/include/SwapChain.h"
#include "../../Graphics/include/DynamicUploadBuffer.h"

#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

namespace EduEngine
{
	class RenderEngine : public IRenderEngine
	{
	public:
		RenderEngine();
		~RenderEngine();

		RenderEngine(const RenderEngine& rhs) = delete;
		RenderEngine& operator=(const RenderEngine& rhs) = delete;

		bool StartUp(const Window& mainWindow);

		void Update(const Timer& timer) override;
		void Render(const Timer& timer) override;

		void PendingResize(UINT w, UINT h);

		static RenderEngine* GetInstance();

	protected:
		virtual void OnStartUp() {};
		virtual void OnUpdate(const Timer& timer) {};
		virtual void OnRender(const Timer& timer) {};

		RenderDeviceD3D12* GetDevice() const { return m_Device.get(); }
		Camera* GetCamera() const { return m_Camera.get(); }
		DeviceContext* GetMainContext() const { return m_MainContext.get(); }

	private:
		void Resize(UINT w, UINT h);

	private:
		static RenderEngine* m_Instance;

		std::unique_ptr<RenderDeviceD3D12> m_Device;
		std::unique_ptr<SwapChain> m_SwapChain;
		std::unique_ptr<Camera> m_Camera;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
		DXGI_ADAPTER_DESC1 m_DeviceDesc;

		static constexpr DirectX::SimpleMath::Rectangle EmptyResize = { -1, -1, -1, -1 };
		DirectX::SimpleMath::Rectangle m_PendingResize = EmptyResize;
	
		std::unique_ptr<DeviceContext> m_MainContext;
	};
}