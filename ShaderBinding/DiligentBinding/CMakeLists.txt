add_library(DiligentBinding STATIC
    src/ComputePipelineStateD3D12.cpp
    src/PipelineStateD3D12.cpp
    src/PipelineStateD3D12Base.cpp
    src/RootParamsManager.cpp
    src/RootSignatureD3D12.cpp
    src/ShaderD3D12.cpp
    src/ShaderResourceCacheD3D12.cpp
    src/ShaderResourceLayoutD3D12.cpp
    src/ShaderResourcesD3D12.cpp
)

target_include_directories(DiligentBinding
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(DiligentBinding PRIVATE UNICODE _UNICODE)
target_compile_definitions(DiligentBinding PUBLIC DILIGENTBINDING_STATIC)

target_link_libraries(DiligentBinding PUBLIC
    Graphics
)