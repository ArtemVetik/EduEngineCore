#pragma once
#include "framework.h"
#include "ResourceViewD3D12.h"
#include "DeviceContext.h"

namespace EduEngine
{
	static inline UINT AlignForUavCounter(UINT bufferSize)
	{
		const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
		return (bufferSize + (alignment - 1)) & ~(alignment - 1);
	}

	class GRAPHICS_API BufferD3D12 : public ResourceViewD3D12
	{
	public:
		BufferD3D12(RenderDeviceD3D12*		   pDevice,
					DeviceContext*			   context,
					const D3D12_RESOURCE_DESC& desc,
					QueueID					   queueId);

		BufferD3D12(RenderDeviceD3D12*		   pDevice,
					DeviceContext*			   context,
					const D3D12_RESOURCE_DESC& desc,
					const void*				   initData,
					QueueID					   queueId);

		void LoadData(const void* data, UINT* byteSize = nullptr);

	private:
		DeviceContext* m_Context;
	};

	class GRAPHICS_API VertexBufferD3D12 : public BufferD3D12
	{
	public:
		VertexBufferD3D12(RenderDeviceD3D12*   pDevice,
						  DeviceContext*	   context,
						  const void*		   initData,
						  UINT				   byteStride,
						  UINT				   bufferLength,
						  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) :
			BufferD3D12(pDevice, context, CD3DX12_RESOURCE_DESC::Buffer(byteStride * bufferLength, flags), initData, QueueID::Direct)
		{
			m_View.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
			m_View.StrideInBytes = byteStride;
			m_View.SizeInBytes = byteStride * bufferLength;
		}

		D3D12_VERTEX_BUFFER_VIEW GetView() const { return m_View; }

	private:
		D3D12_VERTEX_BUFFER_VIEW m_View;
	};

	class GRAPHICS_API IndexBufferD3D12 : public BufferD3D12
	{
	public:
		IndexBufferD3D12(RenderDeviceD3D12*   pDevice,
						 DeviceContext*		  context,
						 const void*		  initData,
						 UINT				  byteStride,
						 UINT				  bufferLength,
						 DXGI_FORMAT		  format,
						 D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) :
			BufferD3D12(pDevice, context, CD3DX12_RESOURCE_DESC::Buffer(byteStride * bufferLength, flags), initData, QueueID::Direct),
			m_Length(bufferLength)
		{
			m_View.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
			m_View.Format = format;
			m_View.SizeInBytes = byteStride * bufferLength;
		}

		D3D12_INDEX_BUFFER_VIEW GetView() const { return m_View; }
		UINT GetLength() const { return m_Length; }

	private:
		UINT m_Length;
		D3D12_INDEX_BUFFER_VIEW m_View;
	};

	class GRAPHICS_API ReadBackBufferD3D12 : public ResourceD3D12
	{
	public:
		ReadBackBufferD3D12(RenderDeviceD3D12* pDevice,
							UINT64			   numElements,
							QueueID			   queueId);

		template <typename T>
		void ReadData(int elementIndex, T& data)
		{
			m_d3d12Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData));

			data = reinterpret_cast<T*>(m_MappedData + elementIndex * sizeof(T))[0];

			m_d3d12Resource->Unmap(0, nullptr);
		}

	private:
		BYTE* m_MappedData = nullptr;
	};

	class GRAPHICS_API UploadBufferD3D12 : public ResourceD3D12
	{
	public:
		UploadBufferD3D12(RenderDeviceD3D12*		 pDevice,
						  const D3D12_RESOURCE_DESC& desc,
						  QueueID					 queueId);

		void LoadData(void* data);
	};
}

