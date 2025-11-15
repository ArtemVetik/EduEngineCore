file(GLOB_RECURSE GRAPHICS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE GRAPHICS_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(EduBinding SHARED
    ${GRAPHICS_SOURCES}
    ${GRAPHICS_HEADERS}
)

target_include_directories(EduBinding PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(EduBinding PRIVATE UNICODE _UNICODE EDUBINDING_EXPORTS)

target_link_libraries(EduBinding PUBLIC
    Graphics
)