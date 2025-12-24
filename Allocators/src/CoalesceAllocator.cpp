#include "../include/CoalesceAllocator.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
#include <iostream>
#include <cassert>
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

#include <cmath>
#include <algorithm>

namespace CoalesceAllocator {
    CoalesceAllocator::CoalesceAllocator() {
        m_headPage = nullptr;
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
        m_allocCallCount = 0;
        m_freeCallCount = 0;
        m_totalAllocSize = 0;
#endif
    }

    CoalesceAllocator::~CoalesceAllocator() {
        if (m_headPage != nullptr)
            destroy();
    }

    void CoalesceAllocator::init() {
        if (m_headPage != nullptr)
            return;

        uint32 binIdx;
        m_headPage = createPage(binIdx);
    }

    void CoalesceAllocator::destroy() {
        ASSERT(m_headPage != nullptr);

        while (m_headPage) {
            Page* next = m_headPage->next;

#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
            uint32 fxIdx = binIndex(sizeof(BlockStart) + PAGE_SIZE + sizeof(BlockEnd));
            ASSERT(m_headPage->fh[fxIdx]->size == sizeof(BlockStart) + PAGE_SIZE + sizeof(BlockEnd));
            ASSERT(m_headPage->fh[fxIdx]->next == nullptr);
#endif

            if (!VirtualFree(m_headPage, 0, MEM_RELEASE)) {
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
                printf("VirtualFree failed.\n");
#endif
                return;
            }

            m_headPage = next;
        }
    }

    void* CoalesceAllocator::alloc(uint32 size) {
        ASSERT(m_headPage != nullptr);

        if (size > PAGE_SIZE)
            return nullptr;

        Page* page = m_headPage;

#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
        m_allocCallCount++;
        m_totalAllocSize += size;
#endif

        while (true) {
            //<--------(fb->size)-------->
            // ↓(fb)
            //[BlockStart][size][BlockEnd]
            uint32 binIdx;
            BlockStart* fb = findFreeBlock(page, sizeof(BlockStart) + size + sizeof(BlockEnd), binIdx);

            if (fb != nullptr) {
                ASSERT(page->fh[binIdx] == fb);
                ASSERT(fb->prev == nullptr);
                validateBlock(fb, true);

                if (fb->next) fb->next->prev = nullptr;
                page->fh[binIdx] = fb->next;

                // <----------------------(fb->size)-------------------------->
                //   ↓(fb)                         ↓(nfb)
                // <[BlockStart][size][BlockEnd]> <[BlockStart][...][BlockEnd]>
                if (fb->size >= size + 2 * sizeof(BlockStart) + 2 * sizeof(BlockEnd)) {
                    auto* nfb = (BlockStart*)((BYTE*)fb + sizeof(BlockStart) + size + sizeof(BlockEnd));
                    uint32 nfbSize = fb->size - size - sizeof(BlockStart) - sizeof(BlockEnd);
                    fb->size -= nfbSize;

                    uint32 nfdBinIdx = binIndex(nfbSize);
                    ASSERT(page->fh[nfdBinIdx] == nullptr || page->fh[nfdBinIdx]->prev == nullptr);
                    setupBlock(nfb, nfbSize, page->fh[nfdBinIdx], nullptr, true);
                    page->fh[nfdBinIdx] = nfb;
                }

                setupBlock(fb, fb->size, nullptr, nullptr, false);
                return (BYTE*)fb + sizeof(BlockStart);
            }

            if (page->next == nullptr)
                break;

            page = page->next;
        }

        uint32 binIdx;
        page->next = createPage(binIdx);
        page = page->next;

        uint32 blockSize = page->fh[binIdx]->size;
        if (blockSize >= size + 2 * sizeof(BlockStart) + 2 * sizeof(BlockEnd)) {
            auto* nfb = (BlockStart*)((BYTE*)page->fh[binIdx] + sizeof(BlockStart) + size + sizeof(BlockEnd));
            setupBlock(nfb, page->fh[binIdx]->size - sizeof(BlockStart) - size - sizeof(BlockEnd), nullptr, nullptr, true);
            blockSize -= nfb->size;
            page->fh[binIdx] = nfb;
        }
        else {
            page->fh[binIdx] = nullptr;
        }

        setupBlock(((BlockStart*)((BYTE*)page + sizeof(Page))), blockSize, nullptr, nullptr, false);
        return (BYTE*)page + sizeof(Page) + sizeof(BlockStart);
    }

    //             ↓(p)
    // [BlockStart][......][BlockEnd]
    void CoalesceAllocator::free(void *p) {
        ASSERT(m_headPage != nullptr);

        Page* page = m_headPage;
        while (page != nullptr) {
            // ↓(page)
            //[Page][BlockStart][..(p)..][BlockEnd]
            if (insidePage(page, p)) {
                validateBlock((BlockStart*)((BYTE*)p - sizeof(BlockStart)), false);
                break;
            }
            page = page->next;
        }

        if (page == nullptr)
            return;

        auto* pageStart = (BYTE*)page + sizeof(Page);

        auto* cb = (BlockStart*)((BYTE*)p - sizeof(BlockStart));
        size_t lbs = (BYTE*)cb == pageStart ? 0 : ((BlockEnd*)((BYTE*)cb - sizeof(BlockEnd)))->size;
        auto* lb = (BlockStart*)((BYTE*)cb - lbs);
        auto* rb = (BlockStart*)((BYTE*)cb + cb->size);

        if (!insidePage(page, (BYTE*)lb + sizeof(BlockStart)) || lb->alloc)
            lb = nullptr;
        if (!insidePage(page, (BYTE*)rb + sizeof(BlockStart)) || rb->alloc)
            rb = nullptr;

        if (lb) validateBlock(lb, true);
        if (rb) validateBlock(rb, true);

        if (lb != nullptr) {
            if (lb->next) lb->next->prev = lb->prev;
            if (lb->prev) lb->prev->next = lb->next;
            else
            {
                ASSERT(page->fh[binIndex(lb->size)] == lb);
                page->fh[binIndex(lb->size)] = lb->next;
            }

            lb->size += cb->size;
            cb = lb;
        }

        if (rb != nullptr) {
            cb->size += rb->size;

            if (rb->next) rb->next->prev = rb->prev;
            if (rb->prev) rb->prev->next = rb->next;
            else
            {
                ASSERT(page->fh[binIndex(rb->size)] == rb);
                page->fh[binIndex(rb->size)] = rb->next;
            }
        }

        uint32 cbBinIdx = binIndex(cb->size);

        if (page->fh[cbBinIdx])
        {
            ASSERT(page->fh[cbBinIdx]->prev == nullptr);
            page->fh[cbBinIdx]->prev = cb;
        }

        cb->next = page->fh[cbBinIdx];
        cb->prev = nullptr;
        page->fh[cbBinIdx] = cb;

        setupBlock(cb, cb->size, cb->next, cb->prev, true);
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
        m_freeCallCount++;
#endif
    }

    CoalesceAllocator::Page *CoalesceAllocator::createPage(uint32& outBinIdx) {
        Page* page = (Page*)VirtualAlloc(nullptr, sizeof(Page) + sizeof(BlockStart) + PAGE_SIZE + sizeof(BlockEnd),
                                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        if (page == nullptr) {
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
            printf("VirtualAlloc failed.\n");
#endif
            return nullptr;
        }

        page->next = nullptr;
        memset(page->fh, 0, sizeof(BlockStart*) * NumBins);

        outBinIdx = binIndex(sizeof(BlockStart) + PAGE_SIZE + sizeof(BlockEnd));
        page->fh[outBinIdx] = (BlockStart*)((BYTE*)page + sizeof(Page));
        setupBlock(page->fh[outBinIdx], sizeof(BlockStart) + PAGE_SIZE + sizeof(BlockEnd), nullptr, nullptr, true);

        return page;
    }

    uint32 CoalesceAllocator::binIndex(uint32 size)
    {
        ASSERT(size > 0);
        return std::min(31u - __lzcnt(size), NumBins - 1); // TODO: make portable
    }

    CoalesceAllocator::BlockStart *CoalesceAllocator::findFreeBlock(const CoalesceAllocator::Page *page, uint32 size, uint32& outBinIdx) {
        for (outBinIdx = binIndex(size); outBinIdx < NumBins; ++outBinIdx) {
            if (page->fh[outBinIdx])
                return page->fh[outBinIdx];
        }

        outBinIdx = -1;
        return nullptr;
    }

    void CoalesceAllocator::setupBlock(BlockStart *block, uint32 size, BlockStart* next, BlockStart* prev, bool free) {
        block->alloc = free ? 0 : 1;
        block->size = size;
        block->next = next;
        block->prev = prev;
        if (block->next) block->next->prev = block;
        if (block->prev) block->prev->next = block;

        auto* end = (BlockEnd*)((BYTE*)block + size - sizeof(BlockEnd));
        end->size = size;
#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
        block->markerStart = FEEDFACE;
        block->markerEnd = FEEDFACE;
        end->markerStart = FEEDFACE;
        end->markerEnd = FEEDFACE;

        if (free) {
            *((uint32*)((BYTE*)block + sizeof(BlockStart))) = DEADBEEF;
        }
#endif
    }

    bool CoalesceAllocator::containsAddress(void *p) const {
        Page* page = m_headPage;
        while(page) {
            if (insidePage(page, p))
                return true;

            page = page->next;
        }

        return false;
    }

    bool CoalesceAllocator::insidePage(Page* page, void *p) {
        return ((BYTE *)p >= (BYTE*)page + sizeof(Page) + sizeof(BlockStart) &&
                (BYTE*) p <= (BYTE*)page + sizeof(Page) + PAGE_SIZE + sizeof(BlockStart) - 1);
    }

    void CoalesceAllocator::validateBlock(BlockStart* block, bool free) {
        ASSERT(block->markerStart == FEEDFACE);
        ASSERT(block->markerEnd == FEEDFACE);
        auto* blockEnd = (BlockEnd*)((BYTE*)block + block->size - sizeof(BlockEnd));
        ASSERT(blockEnd->markerEnd == FEEDFACE);
        ASSERT(blockEnd->markerEnd == FEEDFACE);

        if (free) {
            ASSERT(block->alloc == 0);
            ASSERT(*((uint32*)((BYTE*)block + sizeof(BlockStart))) == DEADBEEF);
        }
        else {
            ASSERT(block->alloc);
        }
    }

#if defined(_DEBUG) && defined(ALLOCATORS_DEBUG)
    StatReport CoalesceAllocator::getStat() const {
        uint32 pagesCount = 0;
        Page* page = m_headPage;
        while (page) pagesCount++, page = page->next;

        return StatReport {
            m_allocCallCount,
            m_freeCallCount,
            m_totalAllocSize,
            pagesCount,
        };
    }

    BlockReport CoalesceAllocator::getNextBlock(uint32 pageNum, void* from) const {
        ASSERT(m_headPage != nullptr);

        Page* page = m_headPage;
        for (int i = 0; i < pageNum && page; ++i, page = page->next);

        if (page == nullptr)
            return BlockReport{};

        if (!from) {
            auto* block = (BlockStart*)((BYTE*)page + sizeof(Page));
            return BlockReport {
                    (BYTE*)block + sizeof(BlockStart),
                    block->size - (uint32)sizeof(BlockStart) - (uint32)sizeof(BlockEnd),
                    block->alloc != 0,
            };
        }

        ASSERT(insidePage(page, from));

        auto* fromBlock = (BlockStart*)((BYTE*)from - sizeof(BlockStart));

        auto* next = (BlockStart*)((BYTE*)fromBlock + fromBlock->size);

        if (!insidePage(page, next))
            return BlockReport{};

        return BlockReport {
                (BYTE*)next + sizeof(BlockStart),
            next->size - (uint32)sizeof(BlockStart) - (uint32)sizeof(BlockEnd),
            next->alloc != 0,
        };
    }

#endif // DEBUG
}
