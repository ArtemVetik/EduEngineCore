file(GLOB_RECURSE RENDER_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
file(GLOB_RECURSE RENDER_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
)
add_library(RenderTechniques STATIC
    ${RENDER_SOURCES}
    ${RENDER_HEADERS})

target_include_directories(RenderTechniques PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(RenderTechniques PRIVATE UNICODE _UNICODE)

#target_compile_definitions(RenderTechniques PRIVATE RENDERTECHNIQUES_EXPORTS)
target_compile_definitions(RenderTechniques PUBLIC RENDERTECHNIQUES_STATIC)

target_link_libraries(RenderTechniques PUBLIC
    Graphics
    EduBinding
)