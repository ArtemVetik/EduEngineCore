set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_library(RenderTechniques STATIC
    src/Camera.cpp
    src/GenerateMipMaps.cpp
    src/IBLRendering.cpp
    src/Timer.cpp
    src/SSAO.cpp
    src/GBuffer.cpp
    src/DeferredPBRLightPass.cpp
    src/CSMRendering.cpp
    src/RenderFeatures.cpp
    src/Mesh.cpp
    src/Texture.cpp
    src/ReflectionProbe.cpp
    src/Skybox.cpp
    src/ScreenSpaceReflection.cpp
    src/VolumetricLight.cpp
)

target_include_directories(RenderTechniques
    PUBLIC 
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${LIBS_ROOT}/include
)

target_compile_definitions(RenderTechniques PRIVATE UNICODE _UNICODE)

#target_compile_definitions(RenderTechniques PRIVATE RENDERTECHNIQUES_EXPORTS)
target_compile_definitions(RenderTechniques PUBLIC RENDERTECHNIQUES_STATIC)

find_library(ASSIMP_LIB
    NAMES assimp-vc143-mt
    HINTS ${LIBS_ROOT}/lib
    NO_DEFAULT_PATH
)

if(NOT ASSIMP_LIB)
    message(FATAL_ERROR "Assimp library 'assimp-vc143-mt.lib' not found in ${LIBS_ROOT}/lib")
endif()

target_link_libraries(RenderTechniques PUBLIC
    Graphics
    EduBinding
    ${ASSIMP_LIB}
)