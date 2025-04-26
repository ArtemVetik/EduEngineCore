file(GLOB_RECURSE INPUT_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE INPUT_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(InputSystem STATIC
    ${INPUT_SOURCES}
    ${INPUT_HEADERS}
)
target_include_directories(InputSystem PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(InputSystem PUBLIC
    user32
)