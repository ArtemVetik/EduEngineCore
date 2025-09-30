#include "pch.h"
#include "BufferD3D12.h"

namespace EduEngine
{
	BufferD3D12::BufferD3D12(RenderDeviceD3D12*			pDevice,
							 const D3D12_RESOURCE_DESC& desc,
							 QueueID					queueId) :
		ResourceViewD3D12(pDevice, queueId)
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

		auto* cmdContext = pDevice->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		cmdContext->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_d3d12Resource.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		cmdContext->FlushResourceBarriers();
	}

	BufferD3D12::BufferD3D12(RenderDeviceD3D12*			pDevice,
							 const D3D12_RESOURCE_DESC& desc,
							 const void*				initData,
							 QueueID					queueId) :
		ResourceViewD3D12(pDevice, queueId)
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

		LoadData(initData);
	}

	void BufferD3D12::LoadData(const void* data, UINT* byteSize /* = nullptr */)
	{
		UINT64 uploadBufferSize = 0;

		if (byteSize)
			uploadBufferSize = *byteSize;
		else
			m_Device->GetD3D12Device()->GetCopyableFootprints(&m_d3d12Resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

		auto uploadBuff = m_Device->AllocateDynamicUploadGPUDescriptor(m_QueueId, uploadBufferSize);

		memcpy(reinterpret_cast<char*>(uploadBuff.CPUAddress), data, uploadBufferSize);

		auto* cmdContext = m_QueueId != QueueID::Direct
			? m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_COMPUTE)
			: m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		auto beforeState = m_QueueId != QueueID::Direct ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_GENERIC_READ;

		cmdContext->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_d3d12Resource.Get(),
			beforeState, D3D12_RESOURCE_STATE_COPY_DEST));
		cmdContext->FlushResourceBarriers();

		cmdContext->GetCmdList()->CopyBufferRegion(m_d3d12Resource.Get(), 0, uploadBuff.pBuffer, uploadBuff.Offset, uploadBufferSize);

		cmdContext->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_d3d12Resource.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, beforeState));
		cmdContext->FlushResourceBarriers();
	}

	ReadBackBufferD3D12::ReadBackBufferD3D12(RenderDeviceD3D12* pDevice,
											 UINT64				numElements,
											 QueueID			queueId) :
		ResourceD3D12(pDevice, queueId)
	{
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(numElements * sizeof(UINT64));

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
										 QueueID					queueId) :
		ResourceD3D12(pDevice, queueId)
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