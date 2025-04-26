cmake_minimum_required(VERSION 3.15)
project(EduEngineCore LANGUAGES CXX)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(EduMath)
add_subdirectory(Graphics.Heaps)
add_subdirectory(Graphics)
add_subdirectory(SampleProject/InputSystem)
add_subdirectory(SampleProject/RenderEngine)
add_subdirectory(SampleProject/EduEngineCore)