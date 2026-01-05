#include "pch.h"
#include "BufferD3D12.h"

namespace EduEngine
{
	BufferD3D12::BufferD3D12(RenderDeviceD3D12*			pDevice,
							 DeviceContext*				context,
							 const D3D12_RESOURCE_DESC& desc,
							 QueueMask					queueMask) :
		ResourceViewD3D12(pDevice, queueMask),
		m_Context(context)
	{
		// Create the actual default buffer resource.
		HRESULT hr = m_Device->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(m_d3d12Resource.GetAddressOf()));

		THROW_IF_FAILED(hr, L"Failed to create resource in default heap");

		m_d3d12Resource->SetName(L"BufferD3D12");

		m_Context->GetCommandCtx()->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_d3d12Resource.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		m_Context->GetCommandCtx()->FlushResourceBarriers();

		SetState(D3D12_RESOURCE_STATE_GENERIC_READ);
	}

	BufferD3D12::BufferD3D12(RenderDeviceD3D12*			pDevice,
							 DeviceContext*				context,
							 const D3D12_RESOURCE_DESC& desc,
							 const void*				initData,
							 QueueMask					queueMask) :
		ResourceViewD3D12(pDevice, queueMask),
		m_Context(context)
	{
		// Create the actual default buffer resource.
		HRESULT hr = m_Device->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(m_d3d12Resource.GetAddressOf()));

		THROW_IF_FAILED(hr, L"Failed to create resource in default heap");

		m_d3d12Resource->SetName(L"BufferD3D12");

		SetState(D3D12_RESOURCE_STATE_GENERIC_READ);

		LoadData(context, initData);
	}

	void BufferD3D12::LoadData(DeviceContext* context, const void* data, UINT* byteSize /* = nullptr */)
	{
		UINT64 uploadBufferSize = 0;

		if (byteSize)
			uploadBufferSize = *byteSize;
		else
			m_Device->GetD3D12Device()->GetCopyableFootprints(&m_d3d12Resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

		DynamicHeapAllocation uploadBuff = context->AllocateDynamicSpace(uploadBufferSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		memcpy(reinterpret_cast<char*>(uploadBuff.GetCpuAddress()), data, uploadBufferSize);

		D3D12_RESOURCE_STATES beforeState = GetState();
		m_Context->GetCommandCtx()->TransitionResource(this, D3D12_RESOURCE_STATE_COPY_DEST, true);
		m_Context->GetCommandCtx()->GetCmdList()->CopyBufferRegion(m_d3d12Resource.Get(), 0, uploadBuff.GetResource(), uploadBuff.GetOffset(), uploadBufferSize);
		m_Context->GetCommandCtx()->TransitionResource(this, beforeState, true);
	}

	ReadBackBufferD3D12::ReadBackBufferD3D12(RenderDeviceD3D12* pDevice,
											 UINT64				width,
											 QueueMask			queueMask) :
		ResourceD3D12(pDevice, queueMask)
	{
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

		HRESULT hr = pDevice->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_d3d12Resource));

		THROW_IF_FAILED(hr, L"Failed to create resource in readback heap");

		m_d3d12Resource->SetName(L"ReadBackBufferD3D12");
	}

	UploadBufferD3D12::UploadBufferD3D12(RenderDeviceD3D12*			pDevice,
										 const D3D12_RESOURCE_DESC& desc,
										 QueueMask					queueMask) :
		ResourceD3D12(pDevice, queueMask)
	{
		HRESULT hr = m_Device->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_d3d12Resource.GetAddressOf()));

		THROW_IF_FAILED(hr, L"Failed to create resource in upload heap");

		m_d3d12Resource->SetName(L"UploadBufferD3D12");
	}

	void UploadBufferD3D12::LoadData(void* data)
	{
		uint8_t* pData;

		m_d3d12Resource->Map(0, nullptr, (void**)&pData);
		memcpy(pData, data, m_d3d12Resource->GetDesc().Width);
		m_d3d12Resource->Unmap(0, nullptr);
	}
}