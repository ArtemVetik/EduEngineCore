add_library(GraphicsHeaps STATIC
    src/CPUDescriptorHeap.cpp
    src/DescriptorHeapAllocation.cpp
    src/DescriptorHeapAllocationManager.cpp
    src/DynamicSuballocationsManager.cpp
    src/DynamicUploadHeap.cpp
    src/GPUDescriptorHeap.cpp
    src/GPURingBuffer.cpp
    src/pch.cpp
    src/RingBuffer.cpp
    src/VariableSizeMemoryAllocator.cpp
    src/DynamicHeap.cpp

    include/ReleaseResourceWrapper.h
)

target_include_directories(GraphicsHeaps
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(GraphicsHeaps PUBLIC
    Allocators
    Common
)

target_compile_definitions(GraphicsHeaps PUBLIC UNICODE _UNICODE GRAPHICSHEAPS_STATIC)

target_precompile_headers(GraphicsHeaps PRIVATE include/pch.h)