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

find_library(ASSIMP_LIB
    NAMES assimp-vc143-mt
    HINTS ${LIBS_ROOT}/lib
    NO_DEFAULT_PATH
)
if(NOT ASSIMP_LIB)
    message(FATAL_ERROR "Assimp library 'assimp-vc143-mt.lib' not found in ${LIBS_ROOT}/lib")
endif()

target_link_libraries(PBR PUBLIC
    Graphics
    Common
    ${ASSIMP_LIB}
)