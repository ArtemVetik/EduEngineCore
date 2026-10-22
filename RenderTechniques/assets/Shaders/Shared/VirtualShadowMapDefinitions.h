#pragma once

#ifndef VSM_GENERATE_STATS
#define VSM_GENERATE_STATS 1
#endif

#define VSM_PAGE_SIZE		128
#define VSM_LOG2_PAGE_SIZE	7
#define VSM_PAGES_XY		128
#define VSM_VIRTUAL_SIZE	(VSM_PAGE_SIZE * VSM_PAGES_XY)
#define VSM_POOL_SIZE		64
#define VSM_MAX_PHYS_PAGES	(VSM_POOL_SIZE * VSM_POOL_SIZE)
#define VSM_MAX_PAGE_AGE	1000

#define VSM_STAT_REQUESTED_PAGES		0  // virtual pages requested this frame (unique)
#define VSM_STAT_CACHED_HITS			1  // requested and already resident -> not rendered
#define VSM_STAT_ALLOCATED_NEW			2  // requested, got a physical page -> rendered
#define VSM_STAT_EVICTED_CACHED			3  // new allocation reused a page that still held cached data
#define VSM_STAT_OVERFLOW				4  // requested, no physical page available
#define VSM_STAT_CACHED_UNREQUESTED		5  // resident but not requested (LRU candidates)
#define VSM_STAT_EMPTY_PAGES			6  // invalid / too old pages
#define VSM_STAT_AVAILABLE_AFTER_PACK	7  // AVAILABLE list size before allocation
#define VSM_STAT_REQUESTED_LIST_FINAL	8  // invariant: must equal VSM_MAX_PHYS_PAGES
#define VSM_STAT_LIST_DUPLICATES		9  // validation: slots seen twice in REQUESTED
#define VSM_STAT_NUM					10