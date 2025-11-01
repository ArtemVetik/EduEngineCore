set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

file(GLOB_RECURSE RENDER_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE RENDER_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(PBR STATIC
    ${RENDER_SOURCES}
    ${RENDER_HEADERS}
)

target_include_directories(PBR PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${LIBS_ROOT}/include
)

target_link_libraries(PBR PRIVATE
    BaseDemo
)