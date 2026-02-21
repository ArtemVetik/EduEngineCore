#pragma once
#include "framework.h"
#include <TextureD3D12.h>

namespace EduEngine
{
	struct TextureLoadDesc
	{
		enum Flags
		{
			CREATE_SRV = 1 << 0,
			//CREATE_UAV = 1 << 1,
			//CREATE_RTV = 1 << 2,
			//CREATE_DSV = 1 << 3,
		};

		D3D12_SHADER_RESOURCE_VIEW_DESC* OverrideDesc = nullptr;
		UINT Flags = CREATE_SRV;
		bool OnCPU = true;
	};

	class RENDERTECHNIQUES_API Texture
	{
	public:
		Texture(RenderDeviceD3D12* device, std::shared_ptr<TextureD3D12> texture, const TextureLoadDesc& loadDesc = {});
		Texture(RenderDeviceD3D12* device, const TextureLoadDesc& loadDesc = {});
		~Texture();

		Texture(const Texture&) = delete;
		Texture& operator =(const Texture&) = delete;

		Texture(Texture&&) = delete;
		Texture& operator =(Texture&&) = delete;

		void* GetGPUPtr();

		void Load(const wchar_t* filePath,
				  DeviceContext* context,
				  const wchar_t* name = nullptr);
		
		std::shared_ptr<TextureD3D12>& GetD3D12Texture() { return m_Texture; }

	private:
		void SetupTexture();

	private:
		RenderDeviceD3D12* m_Device;
		std::shared_ptr<TextureD3D12> m_Texture;

		const TextureLoadDesc m_LoadDesc;
		DescriptorHeapAllocation m_GpuAllocation;
	};
}