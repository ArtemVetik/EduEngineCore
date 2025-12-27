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
    struct MemoryAllocatorT {
        using value_type = T;

        MemoryAllocatorT()
        {
#ifndef USE_STD_ALLOCATIONS
            CompositeMemoryAllocatorSingleton::init();
#endif
        }

        ~MemoryAllocatorT() = default;

        template <typename U>
        MemoryAllocatorT(const MemoryAllocatorT<U>& other) noexcept
        {
        }

        template <typename U>
        MemoryAllocatorT(MemoryAllocatorT<U>&& other)
        {
        }

        template <typename U>
        MemoryAllocatorT& operator = (const MemoryAllocatorT<U>& rhs) = delete;
        template <typename U>
        MemoryAllocatorT& operator = (MemoryAllocatorT<U>&& lhs) = delete;

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
            using other = MemoryAllocatorT<U>;
        };
    };

    template<typename T, typename U>
    bool operator==(const MemoryAllocatorT<T>&, const MemoryAllocatorT<U>&) noexcept
    {
        return true;
    }

    template<typename T, typename U>
    bool operator!=(const MemoryAllocatorT<T>&, const MemoryAllocatorT<U>&) noexcept
    {
        return false;
    }
}