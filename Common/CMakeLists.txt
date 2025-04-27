file(GLOB_RECURSE COMMON_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE COMMON_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.inl"
)
add_library(Common STATIC
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)
target_include_directories(Common PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(Common
    PRIVATE UNICODE _UNICODE
    PUBLIC COMMON_STATIC
)