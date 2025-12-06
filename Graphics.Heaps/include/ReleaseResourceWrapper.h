#pragma once
#include "framework.h"
#include "DescriptorHeapAllocation.h"
#include "DynamicHeap.h"

#include <variant>

namespace EduEngine
{
	struct GRAPHICS_HEAPS_API StaleAllocation
	{
		DescriptorHeapAllocation Allocation;
		IDescriptorAllocator* Heap;

		StaleAllocation() :
			Allocation{},
			Heap{ nullptr }
		{
		}

		StaleAllocation(DescriptorHeapAllocation&& _Allocation, IDescriptorAllocator& _Heap) noexcept :
			Allocation{ std::move(_Allocation) },
			Heap{ &_Heap }
		{
		}

		StaleAllocation(const StaleAllocation&) = delete;
		StaleAllocation& operator= (const StaleAllocation&) = delete;

		StaleAllocation& operator= (StaleAllocation&& rhs) noexcept
		{
			Allocation = std::move(rhs.Allocation);
			Heap = std::move(rhs.Heap);

			rhs.Heap = nullptr;

			return *this;
		}

		StaleAllocation(StaleAllocation&& rhs) noexcept :
			Allocation{ std::move(rhs.Allocation) },
			Heap{ rhs.Heap }
		{
			rhs.Heap = nullptr;
		}

		~StaleAllocation()
		{
			if (Heap != nullptr)
				Heap->FreeAllocation(std::move(Allocation));
		}
	};

	struct GRAPHICS_HEAPS_API StaleDynamicPage
	{
		DynamicHeapPage Page;
		DynamicHeapManager* Manager;

		StaleDynamicPage(DynamicHeapPage&& _Page, DynamicHeapManager* _Manager) noexcept :
			Page{ std::move(_Page) },
			Manager{ _Manager }
		{
		}

		StaleDynamicPage(const StaleDynamicPage&) = delete;
		StaleDynamicPage& operator= (const StaleDynamicPage&) = delete;
		StaleDynamicPage& operator= (StaleDynamicPage&& lhs) noexcept
		{
			Page = std::move(lhs.Page);
			Manager = lhs.Manager;

			lhs.Manager = nullptr;

			return *this;
		}

		StaleDynamicPage(StaleDynamicPage&& rhs)noexcept :
			Page{ std::move(rhs.Page) },
			Manager{ rhs.Manager }
		{
			rhs.Manager = nullptr;
		}

		~StaleDynamicPage()
		{
			if (Manager != nullptr)
			{
				uint64 PageSize = Page.GetSize();
				Manager->m_AvailablePages.emplace(PageSize, std::move(Page));
			}
		}
	};

	class GRAPHICS_HEAPS_API ReleaseResourceWrapper
	{
	public:
		ReleaseResourceWrapper() = default;
		ReleaseResourceWrapper(const ReleaseResourceWrapper&) = delete;
		ReleaseResourceWrapper& operator =(ReleaseResourceWrapper&) = delete;
		ReleaseResourceWrapper& operator =(ReleaseResourceWrapper&& rhs) = delete;

		ReleaseResourceWrapper(ReleaseResourceWrapper&& rhs) noexcept :
			m_Variant(std::move(rhs.m_Variant))
		{
		}

		void AddResource(Microsoft::WRL::ComPtr<ID3D12Resource>&& resource)
		{
			m_Variant = std::move(resource);
		}

		void AddRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>&& signature)
		{
			m_Variant = std::move(signature);
		}

		void AddPageable(Microsoft::WRL::ComPtr<ID3D12Pageable>&& pageable)
		{
			m_Variant = std::move(pageable);
		}

		void AddStaleAllocation(StaleAllocation&& allocation)
		{
			m_Variant = std::move(allocation);
		}

		void AddStaleDynamicPage(StaleDynamicPage&& dynamicPage)
		{
			m_Variant = std::move(dynamicPage);
		}

	private:
		std::variant<
			Microsoft::WRL::ComPtr<ID3D12Resource>,
			Microsoft::WRL::ComPtr<ID3D12RootSignature>,
			Microsoft::WRL::ComPtr<ID3D12Pageable>,
			StaleAllocation,
			StaleDynamicPage
		> m_Variant;
	};
}