set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

file(GLOB_RECURSE RENDER_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE RENDER_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(BaseDemo STATIC
    ${RENDER_SOURCES}
    ${RENDER_HEADERS})

target_include_directories(BaseDemo PUBLIC
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

target_link_libraries(BaseDemo PUBLIC
    RenderTechniques
    Graphics
    DiligentBinding
    EduBinding
    Common
    ImGui
    ${ASSIMP_LIB}
)