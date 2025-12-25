#pragma once
#include <cstddef>
#include <CompositeMemoryAllocator.h>

struct IAllocator
{
    virtual void* alloc(size_t size, size_t align = alignof(std::max_align_t)) = 0;
    virtual void  free(void* ptr) = 0;
    virtual ~IAllocator() = default;
};

struct StdAllocator : IAllocator
{
    void* alloc(size_t size, size_t) override
    {
        return ::operator new(size);
    }

    void free(void* ptr) override
    {
        ::operator delete(ptr);
    }
};

struct CustomAllocator : IAllocator
{
    CompositeMemoryAllocator::CompositeMemoryAllocator m_Allocator;

    CustomAllocator()
    {
        m_Allocator.init();
    }

    ~CustomAllocator()
    {
        m_Allocator.destroy();
    }

    void* alloc(size_t size, size_t) override
    {
        return m_Allocator.alloc(size);
    }

    void free(void* ptr) override
    {
        m_Allocator.free(ptr);
    }
};