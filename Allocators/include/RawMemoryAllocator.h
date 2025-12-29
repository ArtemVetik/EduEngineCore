#pragma once

#include "CompositeMemoryAllocator.h"

#include <memory>

//#define USE_STD_ALLOCATIONS

namespace MemoryAllocator {
    struct CompositeMemoryAllocatorSingleton {
        inline static std::unique_ptr<CompositeMemoryAllocator::CompositeMemoryAllocator> allocator = nullptr;

        static void init() {
            if (!allocator)
            {
                allocator = std::make_unique<CompositeMemoryAllocator::CompositeMemoryAllocator>();
                allocator->init();
            }
        }
    };

    template <typename T>
    struct RawMemoryAllocator {
        using value_type = T;

        RawMemoryAllocator()
        {
#ifndef USE_STD_ALLOCATIONS
            CompositeMemoryAllocatorSingleton::init();
#endif
        }

        ~RawMemoryAllocator() = default;

        template <typename U>
        RawMemoryAllocator(const RawMemoryAllocator<U>& other) noexcept
        {
        }

        template <typename U>
        RawMemoryAllocator(RawMemoryAllocator<U>&& other)
        {
        }

        template <typename U>
        RawMemoryAllocator& operator = (const RawMemoryAllocator<U>& rhs) = delete;
        template <typename U>
        RawMemoryAllocator& operator = (RawMemoryAllocator<U>&& lhs) = delete;

        T* allocate(std::size_t n) {
#ifdef USE_STD_ALLOCATIONS
            return static_cast<T*>(::operator new(n * sizeof(T)));
#else
            if (auto p = CompositeMemoryAllocatorSingleton::allocator->alloc(n * sizeof(T)))
                return static_cast<T*>(p);

            throw std::bad_alloc{};
#endif
        }

        void deallocate(T* p, std::size_t) {
#ifdef USE_STD_ALLOCATIONS
            ::operator delete(p);
#else
            CompositeMemoryAllocatorSingleton::allocator->free(p);
#endif
        }

        template <typename U>
        struct rebind {
            using other = RawMemoryAllocator<U>;
        };
    };

    template<typename T, typename U>
    bool operator==(const RawMemoryAllocator<T>&, const RawMemoryAllocator<U>&) noexcept
    {
        return true;
    }

    template<typename T, typename U>
    bool operator!=(const RawMemoryAllocator<T>&, const RawMemoryAllocator<U>&) noexcept
    {
        return false;
    }
}