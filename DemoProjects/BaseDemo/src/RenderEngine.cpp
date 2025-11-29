#include "RenderEngine.h"

#include <dxgi1_6.h>

#include "../../InputSystem/include/InputManager.h"

namespace EduEngine
{
	RenderEngine* RenderEngine::m_Instance = nullptr;

	RenderEngine* RenderEngine::GetInstance()
	{
		return m_Instance;
	}

	RenderEngine::RenderEngine() :
		m_Viewport{},
		m_ScissorRect{}
	{
		assert(m_Instance == nullptr);
		m_Instance = this;
	}

	RenderEngine::~RenderEngine()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		if (m_Device != nullptr)
			m_Device->FlushQueues();
	}

	bool RenderEngine::StartUp(const Window& mainWindow)
	{
#if defined(DEBUG) || defined(_DEBUG) 
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();
#endif

		IDXGIFactory6* pFactory = nullptr;
		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&pFactory)))
		{
			OutputDebugStringW(L"Failed to create DXGI Factory!");
			return false;
		}

		IDXGIAdapter1* pAdapter = nullptr;

		if (FAILED(pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter1), (void**)&pAdapter)))
		{
			OutputDebugStringW(L"Failed to get high-performance GPU!");
			pFactory->Release();
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		HRESULT hardwareResult = D3D12CreateDevice(
			pAdapter,
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(device.GetAddressOf()));

		if (FAILED(hardwareResult))
		{
			OutputDebugStringW(L"The selected GPU does not support DirectX 12!");
			pAdapter->Release();
			pFactory->Release();
			return false;
		}

		pAdapter->GetDesc1(&m_DeviceDesc);

		pAdapter->Release();
		pFactory->Release();
		m_Device = std::make_unique<RenderDeviceD3D12>(device);

		m_SwapChain = std::make_unique<SwapChain>(m_Device.get(),
			mainWindow.GetClientWidth(), mainWindow.GetClientHeight(), mainWindow.GetMainWindow());

		m_Camera = std::make_unique<Camera>(m_Device.get(), mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		m_MainContext = std::make_unique<DeviceContext>(*m_Device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

		Resize(mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		m_MainContext->GetCommandCtx()->Reset();

		InitImGui(mainWindow);

		ID3D12DescriptorHeap* descriptorHeaps[] = { m_Device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		m_MainContext->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		OnStartUp();

		m_MainContext->GetCommandCtx()->FlushResourceBarriers();
		auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		commandQueue.CloseAndExecuteCommandContext(m_MainContext->GetCommandCtx());
		m_MainContext->GetCommandCtx()->Reset();

		return true;
	}

	void RenderEngine::Update(const Timer& timer)
	{
		OnUpdate(timer);
	}

	void RenderEngine::Render(const Timer& timer)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_Device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

		auto& dCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

		m_MainContext->GetCommandCtx()->Reset();
		m_MainContext->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_MainContext->GetCommandCtx()->FlushResourceBarriers();
		m_MainContext->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		m_MainContext->GetCommandCtx()->SetRenderTargets(1, &m_SwapChain->CurrentBackBufferView(), true, &m_SwapChain->DepthStencilView());
		m_MainContext->GetCommandCtx()->SetViewports(&m_Viewport, 1);
		m_MainContext->GetCommandCtx()->SetScissorRects(&m_ScissorRect, 1);

		const float clear[4] = { 0, 0, 0, 1 };
		m_MainContext->GetCommandCtx()->GetCmdList()->ClearRenderTargetView(m_SwapChain->CurrentBackBufferView(), clear, 0, nullptr);
		m_MainContext->GetCommandCtx()->GetCmdList()->ClearDepthStencilView(m_SwapChain->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
	
		///////////
		OnRender(timer);
		///////////

		m_MainContext->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		m_MainContext->GetCommandCtx()->FlushResourceBarriers();

		dCommandQueue.CloseAndExecuteCommandContext(m_MainContext->GetCommandCtx());
		m_MainContext->GetCommandCtx()->Reset();

		m_SwapChain->Present();
		m_Device->FinishFrame();

		// Resize should be at the end of the frame after the main ExecuteCommandList and FinishFrame.
		// Since FinishFrame also occurs inside swapChain->Resize(), and it is not desirable
		// that resources are removed before the end of rendering
		if (m_PendingResize != EmptyResize)
		{
			Resize(m_PendingResize.width, m_PendingResize.height);
			m_PendingResize = EmptyResize;
		}
	}

	void RenderEngine::PendingResize(UINT w, UINT h)
	{
		UINT lx, ly, lw, lh;
		Window::GetInstance()->GetPosition(lx, ly, lw, lh);

		if (lw != w || lh != h)
			m_PendingResize = { (long)lx, (long)ly, (long)w, (long)h };
	}
	
	void RenderEngine::AllocImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
	{
		m_ImGuiTex = m_Device->AllocateGPUDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		*out_cpu_handle = m_ImGuiTex.GetCpuHandle();
		*out_gpu_handle = m_ImGuiTex.GetGpuHandle();
	}

	void RenderEngine::FreeImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
	{
		m_ImGuiTex = {};
	}

	void RenderEngine::InitImGui(const Window& mainWindow)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(mainWindow.GetMainWindow());

		ImGui_ImplDX12_InitInfo init_info;
		init_info.Device = m_Device->GetD3D12Device();
		init_info.CommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetD3D12CommandQueue();
		init_info.NumFramesInFlight = 3;
		init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		init_info.SrvDescriptorHeap = m_Device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
			{
				return RenderEngine::GetInstance()->AllocImGuiSrv(info, out_cpu_handle, out_gpu_handle);
			};

		init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
			{
				return RenderEngine::GetInstance()->FreeImGuiSrv(info, cpu_handle, gpu_handle);
			};

		ImGui_ImplDX12_Init(&init_info);
	}

	void RenderEngine::Resize(UINT w, UINT h)
	{
		auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

		m_MainContext->GetCommandCtx()->Reset();
		m_SwapChain->Resize(m_MainContext.get(), w, h);

		m_MainContext->GetCommandCtx()->FlushResourceBarriers();
		commandQueue.CloseAndExecuteCommandContext(m_MainContext->GetCommandCtx());
		m_MainContext->GetCommandCtx()->Reset();

		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;
		m_Viewport.Width = w;
		m_Viewport.Height = h;
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;

		m_ScissorRect = { 0, 0, (int)w, (int)h };

		m_Camera->SetProjectionMatrix(w, h);
	}
}