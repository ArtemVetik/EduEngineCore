add_subdirectory(lib)
include_directories(${gtest_SOURCE_DIR}/include {gtest_SOURCE_DIR}/scr ${gtest_SOURCE_DIR})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(Google_Tests_run
            FixedSizeAllocatorTests.cpp
            CoalesceAllocatorTests.cpp
            CompositeMemoryAllocatorTests.cpp
    )

    target_link_libraries(Google_Tests_run Allocators gtest gtest_main)
endif()