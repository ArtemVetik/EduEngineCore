#pragma once

#include "IRenderEngine.h"
#include "RenderPasses.h"
#include "Timer.h"
#include "Camera.h"
#include "Window.h"
#include "RenderObject.h"

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

	private:
		void Resize(UINT w, UINT h);

	private:
		static RenderEngine* m_Instance;

		std::unique_ptr<RenderDeviceD3D12> m_Device;
		std::unique_ptr<SwapChain> m_SwapChain;

		std::unique_ptr<Camera> m_Camera;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_LightBuffer;
		std::unique_ptr<ForwardOpaque> m_ColorPass;

		std::shared_ptr<Texture> m_Texture;
		std::shared_ptr<Mesh> m_Mesh;
		std::shared_ptr<Material> m_Material;
		std::shared_ptr<RenderObject> m_RenderObject;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
		DXGI_ADAPTER_DESC1 m_DeviceDesc;

		static constexpr DirectX::SimpleMath::Rectangle EmptyResize = { -1, -1, -1, -1 };
		DirectX::SimpleMath::Rectangle m_PendingResize = EmptyResize;
	};
}