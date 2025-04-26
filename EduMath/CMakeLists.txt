file(GLOB_RECURSE EDUMATH_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE EDUMATH_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.inl"
)
add_library(EduMath STATIC
    ${EDUMATH_SOURCES}
    ${EDUMATH_HEADERS}
)
target_include_directories(EduMath PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)