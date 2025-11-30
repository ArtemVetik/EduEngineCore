set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_library(BaseDemo STATIC
    src/DebugRendererSystem.cpp
    src/IRenderEngine.cpp
    src/Mesh.cpp
    src/RenderEngine.cpp
    src/Texture.cpp
    src/Window.cpp
)

target_include_directories(BaseDemo 
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${LIBS_ROOT}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/internal
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