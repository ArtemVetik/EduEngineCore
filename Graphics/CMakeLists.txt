add_library(Graphics STATIC
    src/BufferD3D12.cpp
    src/CommandContext.cpp
    src/CommandListManager.cpp
    src/CommandQueueD3D12.cpp
    src/CommandSignatureD3D12.cpp
    src/DDSTextureLoader.cpp
    src/DeviceContext.cpp
    src/DynamicUploadBuffer.cpp
    
    src/pch.cpp
    src/QueryHeap.cpp
    src/RenderDeviceD3D12.cpp
    src/ResourceViewD3D12.cpp
    src/stb_image_impl.cpp
    src/SwapChain.cpp
    src/TextureD3D12.cpp
    src/CommandContextPool.cpp
)

target_include_directories(Graphics
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(Graphics PRIVATE UNICODE _UNICODE)
target_compile_definitions(Graphics PUBLIC GRAPHICS_STATIC)

target_link_libraries(Graphics PUBLIC
    GraphicsHeaps
    Common
    d3d12
    dxgi
    d3dcompiler
    dxguid
)

target_precompile_headers(GraphicsHeaps PRIVATE include/pch.h)