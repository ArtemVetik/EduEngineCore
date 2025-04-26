file(GLOB_RECURSE GRAPHICS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE GRAPHICS_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(Graphics SHARED
    ${GRAPHICS_SOURCES}
    ${GRAPHICS_HEADERS}
  )
target_include_directories(Graphics PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Ensure UNICODE build so LPCTSTR maps to wchar_t*
target_compile_definitions(Graphics PRIVATE UNICODE _UNICODE GRAPHICS_EXPORTS)

target_link_libraries(Graphics PUBLIC
    GraphicsHeaps
    EduMath
    d3d12
    dxgi
    d3dcompiler
    dxguid
)