#pragma once
#include "framework.h"
#include "RenderDeviceD3D12.h"

#include <QueueMask.h>

namespace EduEngine
{
	class GRAPHICS_API ResourceD3D12
	{
	public:
		ResourceD3D12(RenderDeviceD3D12* pDevice, QueueMask queueMask) :
			m_Device(pDevice),
			m_QueueMask(queueMask),
			m_UsageState(D3D12_RESOURCE_STATE_COMMON)
		{}

		ResourceD3D12(RenderDeviceD3D12* pDevice, Microsoft::WRL::ComPtr<ID3D12Resource>& resource, QueueMask queueMask) :
			m_Device(pDevice),
			m_d3d12Resource(std::move(resource)),
			m_QueueMask(queueMask),
			m_UsageState(D3D12_RESOURCE_STATE_COMMON)
		{}

		virtual ~ResourceD3D12()
		{
			ReleaseResourceWrapper releaseResource;
			releaseResource.Set(std::move(m_d3d12Resource));

			m_Device->SafeReleaseObject(std::move(releaseResource), m_QueueMask);
		}

		D3D12_RESOURCE_STATES GetState() const { return m_UsageState; }
		void SetState(D3D12_RESOURCE_STATES usageState) { m_UsageState = usageState; }
		bool CheckAllStates(D3D12_RESOURCE_STATES states) const { return (m_UsageState & states) == states; }
		bool CheckAnyState(D3D12_RESOURCE_STATES states) const { return (m_UsageState & states) != 0; }

		ID3D12Resource* GetD3D12Resource() const { return m_d3d12Resource.Get(); }

		void SetName(LPCWSTR name) { m_d3d12Resource->SetName(name); }

	protected:
		D3D12_RESOURCE_STATES m_UsageState;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_d3d12Resource;
		RenderDeviceD3D12* m_Device;
		QueueMask m_QueueMask;
	};
}