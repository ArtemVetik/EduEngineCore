#pragma once
#include "framework.h"
#include "DescriptorHeapAllocation.h"
#include "DynamicHeap.h"

#include <Asserts.h>
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

	struct GRAPHICS_HEAPS_API ResourceHolder
	{
		std::variant<
			Microsoft::WRL::ComPtr<ID3D12Resource>,
			Microsoft::WRL::ComPtr<ID3D12RootSignature>,
			Microsoft::WRL::ComPtr<ID3D12Pageable>,
			StaleAllocation,
			StaleDynamicPage
		> Variant;

		virtual ~ResourceHolder() = default;

		virtual void AddRef() = 0;
		virtual uint32 Release() = 0;
	};

	struct GRAPHICS_HEAPS_API SingleResourceHolder : public ResourceHolder
	{
		~SingleResourceHolder() = default;

		void AddRef() override {}

		uint32 Release() override
		{
			return 1;
		}
	};

	struct GRAPHICS_HEAPS_API SharedResourceHolder : public ResourceHolder
	{
		std::atomic<uint32> RefCount{ 1 };

		SharedResourceHolder() = default;
		~SharedResourceHolder() = default;

		void AddRef() override
		{
			RefCount.fetch_add(1, std::memory_order_relaxed);
		}

		uint32 Release() override
		{
			return RefCount.fetch_sub(1, std::memory_order_acq_rel);
		}
	};

	class GRAPHICS_HEAPS_API ReleaseResourceWrapper
	{
	public:
		ReleaseResourceWrapper(QueueMask queueMask) :
			m_Ptr(nullptr),
			m_QueueMask(queueMask)
		{
		}

		ReleaseResourceWrapper(const ReleaseResourceWrapper& rhs) noexcept :
			m_Ptr(rhs.m_Ptr),
			m_QueueMask(rhs.m_QueueMask)
		{
			AddRef();
		}

		ReleaseResourceWrapper& operator=(const ReleaseResourceWrapper& rhs) noexcept
		{
			if (this == &rhs)
				return *this;

			Release();

			m_Ptr = rhs.m_Ptr;
			m_QueueMask = rhs.m_QueueMask;
			AddRef();

			return *this;
		}

		ReleaseResourceWrapper(ReleaseResourceWrapper&& rhs) noexcept :
			m_Ptr(rhs.m_Ptr),
			m_QueueMask(rhs.m_QueueMask)
		{
			rhs.m_Ptr = nullptr;
		}

		ReleaseResourceWrapper& operator=(ReleaseResourceWrapper&& rhs) noexcept
		{
			if (this == &rhs)
				return *this;

			Release();

			m_Ptr = rhs.m_Ptr;
			m_QueueMask = rhs.m_QueueMask;

			rhs.m_Ptr = nullptr;

			return *this;
		}

		~ReleaseResourceWrapper()
		{
			Release();
		}

		template<typename T>
		void Set(T&& obj)
		{
			Release();

			VERIFY_EXPR(m_QueueMask > 0, "");

			bool isPowerOfTwo = (m_QueueMask & (m_QueueMask - 1)) == 0;

			// If it is used only in one queue
			if (isPowerOfTwo)
				m_Ptr = new SingleResourceHolder();
			else
				m_Ptr = new SharedResourceHolder();
			
			m_Ptr->Variant = std::move(obj);
		}

		void ReleaseOwnership()
		{
			if (m_Ptr)
				m_Ptr->Release();

			m_Ptr = nullptr;
		}

		QueueMask GetQueueMask() const { return m_QueueMask; }

	private:
		void AddRef()
		{
			if (m_Ptr)
				m_Ptr->AddRef();
		}

		void Release()
		{
			if (!m_Ptr)
				return;

			if (m_Ptr->Release() == 1)
			{
				delete m_Ptr;
			}

			m_Ptr = nullptr;
		}

	private:
		ResourceHolder* m_Ptr = nullptr;
		QueueMask m_QueueMask;
	};
}