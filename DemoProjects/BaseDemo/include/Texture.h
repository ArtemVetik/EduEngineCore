#pragma once

#include "../../Graphics/include/TextureD3D12.h"

namespace EduEngine
{
	class Texture
	{
	public:
		Texture();
		~Texture();

		void* GetGPUPtr();

		void Load(const wchar_t* filePath,
				  RenderDeviceD3D12* device,
				  DeviceContext* context,
				  D3D12_SHADER_RESOURCE_VIEW_DESC* overrideDesc = nullptr,
				  wchar_t* name = nullptr);
		
		std::shared_ptr<TextureD3D12>& GetD3D12Texture() { return m_Texture; }

	private:
		RenderDeviceD3D12* m_Device;
		std::shared_ptr<TextureD3D12> m_Texture;

		DescriptorHeapAllocation m_GpuAllocation;
	};
}