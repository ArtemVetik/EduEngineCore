cmake_minimum_required(VERSION 3.15)
project(EduEngineCore LANGUAGES CXX)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(Common)
add_subdirectory(ImGui)
add_subdirectory(Graphics.Heaps)
add_subdirectory(Graphics)
add_subdirectory(InputSystem)
add_subdirectory(DemoProjects/BaseDemo)
add_subdirectory(DemoProjects/PBR)
add_subdirectory(ShaderBinding/DiligentBinding)
add_subdirectory(ShaderBinding/EduBinding)
add_subdirectory(EduEngineCore)