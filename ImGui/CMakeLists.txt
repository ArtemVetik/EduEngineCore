cmake_minimum_required(VERSION 3.15)

project(ImGui LANGUAGES C CXX)

add_library(ImGui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp

    imgui/imgui_demo.cpp

    imgui/backends/imgui_impl_win32.cpp
    imgui/backends/imgui_impl_dx12.cpp
)

target_include_directories(ImGui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

target_compile_features(ImGui PUBLIC cxx_std_17)

target_compile_definitions(ImGui PUBLIC
    IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
    IMGUI_ENABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
)

target_link_libraries(ImGui PUBLIC
    D3D12
    dxgi
    d3dcompiler
)

# target_compile_definitions(ImGui PUBLIC IMGUI_USER_CONFIG="imgui_config.h")