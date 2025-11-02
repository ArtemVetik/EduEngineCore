file(GLOB_RECURSE GRAPHICS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE GRAPHICS_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(DiligentBinding SHARED
    ${GRAPHICS_SOURCES}
    ${GRAPHICS_HEADERS}
  )
target_include_directories(DiligentBinding PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(DiligentBinding PRIVATE UNICODE _UNICODE DILIGENTBINDING_EXPORTS)

target_link_libraries(DiligentBinding PUBLIC
    Graphics
)