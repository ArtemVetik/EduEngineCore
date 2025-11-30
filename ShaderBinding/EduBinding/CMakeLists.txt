add_library(EduBinding STATIC
    src/ComputePipelineState.cpp
    src/PipelineState.cpp
    src/PipelineStateBase.cpp
    src/RootSignature.cpp
    src/ShaderBinder.cpp
    src/ShaderD3D12.cpp
    src/ShaderResources.cpp
)

target_include_directories(EduBinding
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(EduBinding PRIVATE UNICODE _UNICODE)

#target_compile_definitions(EduBinding PRIVATE EDUBINDING_EXPORTS)
target_compile_definitions(EduBinding PUBLIC EDUBINDING_STATIC)

#target_compile_definitions(EduBinding PUBLIC $<$<CONFIG:Debug>:EDUBINDINGDEBUG>)

target_link_libraries(EduBinding PUBLIC
    Graphics
)