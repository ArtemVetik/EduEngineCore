#pragma once
#include "framework.h"
#include "DescriptorHeapAllocation.h"
#include "DynamicHeap.h"

#include <Asserts.h>
#include <RawMemoryAllocator.h>
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
		ReleaseResourceWrapper& operator=(const ReleaseResourceWrapper&) = delete;

		ReleaseResourceWrapper(ReleaseResourceWrapper&& rhs) noexcept :
			m_Variant(std::move(rhs.m_Variant))
		{ }

		ReleaseResourceWrapper& operator=(ReleaseResourceWrapper&& rhs) noexcept
		{
			if (this == &rhs)
				return *this;

			m_Variant = std::move(rhs.m_Variant);
			return *this;
		}

		template<typename T>
		void Set(T&& obj)
		{
			m_Variant = std::move(obj);
		}

	private:
		std::variant<
			Microsoft::WRL::ComPtr<ID3D12Resource>,
			Microsoft::WRL::ComPtr<ID3D12RootSignature>,
			Microsoft::WRL::ComPtr<ID3D12Pageable>,
			StaleAllocation,
			StaleDynamicPage
		> m_Variant;

#ifdef _DEBUG
	public:
		uint32 GetVariantIndex() { return m_Variant.index(); }

		std::string GetVariantDebugStr()
		{
			static constexpr const char* VariantTypeNames[] = {
				"Resource",
				"RootSignature",
				"Pageable",
				"StaleAllocation",
				"StaleDynamicPage"
			};

			size_t idx = m_Variant.index();
			
			if (idx < std::size(VariantTypeNames))
				return VariantTypeNames[idx];

			return "UnknownVariant";
		}
#endif
	};

	class GRAPHICS_HEAPS_API SingleReleaseResource
	{
	public:
		SingleReleaseResource(ReleaseResourceWrapper&& wrapper) :
			m_Res(std::move(wrapper))
		{}

		virtual ~SingleReleaseResource() = default;

		virtual uint32 Release()
		{
			return 1;
		}

	private:
		ReleaseResourceWrapper m_Res;

#ifdef _DEBUG
	public:
		uint32 GetVariantIndex() { return m_Res.GetVariantIndex(); }
		std::string GetVariantDebugStr() { return m_Res.GetVariantDebugStr(); }
#endif
	};

	class GRAPHICS_HEAPS_API SharedReleaseResource : public SingleReleaseResource
	{
	public:
		SharedReleaseResource(ReleaseResourceWrapper&& wrapper, uint32 numReferences) :
			SingleReleaseResource(std::move(wrapper)),
			m_RefCount(numReferences)
		{ }

		~SharedReleaseResource() = default;

		uint32 Release() override
		{
			return m_RefCount.fetch_sub(1, std::memory_order_acq_rel);
		}

	private:
		std::atomic<uint32> m_RefCount{ 1 };

#ifdef _DEBUG
	public:
		uint32 GetRefCount() { return m_RefCount.load(); }
#endif
	};

	class GRAPHICS_HEAPS_API ReleaseResource
	{
	public:
		ReleaseResource(ReleaseResourceWrapper&& wrapper, uint32 numReferences)
		{
			if (numReferences == 1)
			{
				m_Res = static_cast<SingleReleaseResource*>(MemoryAllocator::CompositeMemoryAllocatorSingleton::allocator->alloc(sizeof(SingleReleaseResource)));
				new (m_Res) SingleReleaseResource(std::move(wrapper));
			}
			else
			{
				m_Res = static_cast<SharedReleaseResource*>(MemoryAllocator::CompositeMemoryAllocatorSingleton::allocator->alloc(sizeof(SharedReleaseResource)));
				new (m_Res) SharedReleaseResource(std::move(wrapper), numReferences);
			}
		}

		ReleaseResource(const ReleaseResource& rhs) :
			m_Res(rhs.m_Res)
		{ }

		ReleaseResource(ReleaseResource&& lhs) noexcept :
			m_Res(lhs.m_Res)
		{
			lhs.m_Res = nullptr;
		}

		ReleaseResource& operator = (const ReleaseResource&) = delete;
		ReleaseResource& operator = (ReleaseResource&&) = delete;

		~ReleaseResource()
		{
			if (!m_Res)
				return;

			if (m_Res->Release() == 1)
			{
				m_Res->~SingleReleaseResource();
				MemoryAllocator::CompositeMemoryAllocatorSingleton::allocator->free(m_Res);

				m_Res = nullptr;
			}
		}

		void ReleaseOwnership()
		{
			m_Res = nullptr;
		}

	private:
		SingleReleaseResource* m_Res;

#ifdef _DEBUG
	public:
		std::string GetReleaseResourceDebugStr()
		{
			if (!m_Res)
				return "NULL";

			std::string result;

			if (auto singlePtr = dynamic_cast<SingleReleaseResource*>(m_Res))
			{
				result += "Single | ";
				result += "Idx: ";
				result += std::to_string(singlePtr->GetVariantIndex());
				result += " | Type: ";
				result += singlePtr->GetVariantDebugStr();
			}
			else if (auto sharedPtr = dynamic_cast<SharedReleaseResource*>(m_Res))
			{
				result += "Shared | RefCount: ";
				result += std::to_string(sharedPtr->GetRefCount());
				result += " | Idx: ";
				result += std::to_string(sharedPtr->GetVariantIndex());
				result += " | Type: ";
				result += sharedPtr->GetVariantDebugStr();
			}
			else
			{
				return "UNKNOWN TYPE";
			}

			return result;
		}
#endif
	};
}