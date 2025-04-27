file(GLOB_RECURSE HEAPS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE HEAPS_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(GraphicsHeaps STATIC
    ${HEAPS_SOURCES}
    ${HEAPS_HEADERS}
)
target_include_directories(GraphicsHeaps PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(GraphicsHeaps PUBLIC UNICODE _UNICODE GRAPHICSHEAPS_STATIC)

target_precompile_headers(GraphicsHeaps PRIVATE include/pch.h)