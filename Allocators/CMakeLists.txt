
add_library(Allocators STATIC
    src/CoalesceAllocator.cpp
    src/CompositeMemoryAllocator.cpp
    src/FixedSizeAllocator.cpp
)

target_include_directories(Allocators
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(Allocators PUBLIC ALLOCATORS_DEBUG)

add_test(
    NAME AllocatorsTests
    COMMAND Google_Tests_run
)

add_subdirectory(google-tests)
add_subdirectory(benchmarks)