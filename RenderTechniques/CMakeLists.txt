add_library(RenderTechniques STATIC
    src/Camera.cpp
    src/GenerateMipMaps.cpp
    src/PBRPrepass.cpp
    src/Timer.cpp
    src/SSAO.cpp
    src/GBuffer.cpp
    src/DeferredPBRLightPass.cpp
)

target_include_directories(RenderTechniques
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(RenderTechniques PRIVATE UNICODE _UNICODE)

#target_compile_definitions(RenderTechniques PRIVATE RENDERTECHNIQUES_EXPORTS)
target_compile_definitions(RenderTechniques PUBLIC RENDERTECHNIQUES_STATIC)

target_link_libraries(RenderTechniques PUBLIC
    Graphics
    EduBinding
)