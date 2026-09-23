add_subdirectory(lib)
include_directories(${gtest_SOURCE_DIR}/include)

# The tests check the allocators' debug checks with death tests, so they need a Debug build
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(Google_Tests_run
            FixedSizeAllocatorTests.cpp
            CoalesceAllocatorTests.cpp
            CompositeMemoryAllocatorTests.cpp
    )

    target_link_libraries(Google_Tests_run Allocators gtest gtest_main)

    add_test(
        NAME AllocatorsTests
        COMMAND Google_Tests_run
    )
endif()