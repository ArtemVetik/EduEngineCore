#pragma once
#include "framework.h"
#include "TextureD3D12.h"

namespace EduEngine
{
	class RENDERTECHNIQUES_API GBuffer
	{
	public:
		GBuffer(int gBufferCount, const DXGI_FORMAT* formats, int accumCount, DXGI_FORMAT accumBuffFormat);

		void Resize(RenderDeviceD3D12* device, DeviceContext* context, UINT width, UINT height);

		TextureD3D12* GetGBuffer(int index) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetGBufferRTVView(int index) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGBufferSRVView(int index) const;

		TextureD3D12* GetAccumBuffer(int index) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetAccumBuffRTVView(int index) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetAccumBuffSRVView(int index) const;

		static constexpr int MaxGBufferCount = 8;
		static constexpr int MaxAccumBufferCount = 2;
	private:
		int m_gBufferCount;
		int m_accumCount;
		DXGI_FORMAT m_Formats[MaxGBufferCount];
		DXGI_FORMAT m_AccumBuffFormat;

		std::shared_ptr<TextureD3D12> m_GBuffers[MaxGBufferCount];
		std::shared_ptr<TextureD3D12> m_AccumBuffer[MaxAccumBufferCount];
	};
}