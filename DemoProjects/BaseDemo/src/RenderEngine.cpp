#include "RenderEngine.h"

#include <dxgi1_6.h>

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

		m_InitInfo = {};
		ChangeInitInfo(m_InitInfo);

		m_Device = std::make_unique<RenderDeviceD3D12>(device, m_InitInfo.QueuesCount);
		m_SwapChain = std::make_unique<SwapChain>(m_Device.get(),
			mainWindow.GetClientWidth(), mainWindow.GetClientHeight(), mainWindow.GetMainWindow());

		m_Camera = std::make_unique<Camera>(m_Device.get(), mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		m_MainContext = std::make_unique<DeviceContext>(*m_Device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

		if (m_InitInfo.NumDeferredContexts > RenderDeviceD3D12::MaxDeviceContexts)
		{
			ASSERT_FAILED("Maximum number of deferred contexts has been exceeded. (", m_InitInfo.NumDeferredContexts, ">", RenderDeviceD3D12::MaxDeviceContexts, ")");
			return false;
		}

		for (uint16 i = 0; i < m_InitInfo.NumDeferredContexts; i++)
			m_DeferredContexts.emplace_back(std::make_unique<DeviceContext>(*m_Device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT));

		Resize(mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		m_MainContext->GetCommandCtx()->Reset();

		InitImGui(mainWindow);

		ID3D12DescriptorHeap* descriptorHeaps[] = { m_Device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		m_MainContext->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		OnStartUp();

		m_MainContext->GetCommandCtx()->FlushResourceBarriers();
		auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

		CommandContext* contexts[]{ m_MainContext->GetCommandCtx() };
		commandQueue.CloseAndExecuteCommandContexts(contexts, 1);
		m_MainContext->FinishFrame();
		m_MainContext->GetCommandCtx()->Reset();

		return true;
	}

	void RenderEngine::Update(const Timer& timer)
	{
		OnUpdate(timer);
	}

	void RenderEngine::Render(const Timer& timer)
	{
		OnRender(timer);

		m_MainContext->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		m_MainContext->GetCommandCtx()->FlushResourceBarriers();

		CommandContext* contexts[]{ m_MainContext->GetCommandCtx() };
		auto& dCommandQueue = GetDevice()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		dCommandQueue.CloseAndExecuteCommandContexts(contexts, 1);

		m_MainContext->FinishFrame();
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
		m_ImGuiTex = m_Device->AllocateGPUDescriptor(QueueId::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
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

		CommandContext* contexts[]{ m_MainContext->GetCommandCtx() };
		commandQueue.CloseAndExecuteCommandContexts(contexts, 1);
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

	DeviceContext* RenderEngine::GetDeferredContext(uint16 idx) const
	{
		VERIFY_EXPR(idx < m_InitInfo.NumDeferredContexts, "The maximum possible number of deferred contexts is ", m_InitInfo.NumDeferredContexts,
			". It is impossible to take the context with the index: ", idx);

		return m_DeferredContexts[idx].get();
	}

	void RenderEngine::PopulateDebugImguiCommand()
	{
		ImGui::Begin("Release Queue Debug");

#ifdef _DEBUG
		ImGui::Text(m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetDebugReleaseQueueStr().c_str());
		//ImGui::Text(m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE).GetDebugReleaseQueueStr().c_str());
		//ImGui::Text(m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY).GetDebugReleaseQueueStr().c_str());
#else
		ImGui::Text("Works only in Debug mode!");
#endif

		ImGui::End();
	}
}