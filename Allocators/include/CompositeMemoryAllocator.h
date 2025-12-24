#ifndef COMPOSITE_MEMORY_ALLOCATOR_COMPOSITEMEMORYALLOCATOR_H
#define COMPOSITE_MEMORY_ALLOCATOR_COMPOSITEMEMORYALLOCATOR_H


#include "FixedSizeAllocator.h"
#include "CoalesceAllocator.h"

#include <memory>

namespace CompositeMemoryAllocator {
    typedef unsigned int uint32;

    static constexpr uint32 BLOCK_TYPE_COUNT = 6;

    enum FSABlockSize {
        FSA16 = 4,
        FSA32 = 5,
        FSA64 = 6,
        FSA128 = 7,
        FSA256 = 8,
        FSA512 = 9,
    };

    class CompositeMemoryAllocator {
    public:
        CompositeMemoryAllocator() = default;
        ~CompositeMemoryAllocator() = default;

        CompositeMemoryAllocator(const CompositeMemoryAllocator&) = delete;
        CompositeMemoryAllocator& operator = (const CompositeMemoryAllocator&) = delete;
        CompositeMemoryAllocator(CompositeMemoryAllocator&&) = delete;
        CompositeMemoryAllocator& operator = (CompositeMemoryAllocator&&) = delete;

        void init();
        void destroy();
        void* alloc(uint32 size);
        void free(void *p);
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
        void dumpStat() const;
        void dumpBlocks() const;
#endif
    private:
        struct VirtualAllocPage {
            VirtualAllocPage* next;
            VirtualAllocPage* prev;
        };

        FixedSizeAllocator::FixedSizeAllocator m_fixedSizeAllocators[BLOCK_TYPE_COUNT];
        CoalesceAllocator::CoalesceAllocator m_coalesceAllocator;
        VirtualAllocPage* m_virtualAllocHead = nullptr;
    };
    
    struct CompositeMemoryAllocatorSingleton {
        inline static std::unique_ptr<CompositeMemoryAllocator> allocator = nullptr;

        static void init() {
            if (!allocator) 
            {
                allocator = std::make_unique<CompositeMemoryAllocator>();
                allocator->init();
            }
        }
    };

    template <typename T>
    struct CompositeMemoryAllocatorT {
        using value_type = T;

        CompositeMemoryAllocatorT()
        {
            CompositeMemoryAllocatorSingleton::init();
        }

        ~CompositeMemoryAllocatorT()
        {
            int aaa = 123;
            //allocator
        }

        template <typename U>
        CompositeMemoryAllocatorT(const CompositeMemoryAllocatorT<U>& other) noexcept
            //: allocator(other.allocator)
        {
        }

        template <typename U>
        CompositeMemoryAllocatorT(CompositeMemoryAllocatorT<U>&& other) //:
            //allocator(std::move(other.allocator))
        {
        }

        template <typename U>
        CompositeMemoryAllocatorT& operator = (const CompositeMemoryAllocatorT<U>& rhs) = delete;
        template <typename U>
        CompositeMemoryAllocatorT& operator = (CompositeMemoryAllocatorT<U>&& lhs) = delete;

        T* allocate(std::size_t n) {
            if (auto p = CompositeMemoryAllocatorSingleton::allocator->alloc(n * sizeof(T)))
                return static_cast<T*>(p);

            throw std::bad_alloc{};
        }

        void deallocate(T* p, std::size_t) {
            CompositeMemoryAllocatorSingleton::allocator->free(p);
        }

        template <typename U>
        struct rebind {
            using other = CompositeMemoryAllocatorT<U>;
        };
    };
}


#endif //COMPOSITE_MEMORY_ALLOCATOR_COMPOSITEMEMORYALLOCATOR_H
