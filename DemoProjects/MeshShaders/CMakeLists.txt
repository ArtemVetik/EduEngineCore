set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(MeshShaders WIN32 src/main.cpp)

target_sources(MeshShaders PRIVATE
    src/MeshShadersDemo.cpp
    src/MeshShadersDemoFactory.cpp
)

target_include_directories(MeshShaders
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(MeshShaders PRIVATE
    BaseDemo
)

add_custom_command(TARGET MeshShaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:MeshShaders>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:MeshShaders>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:MeshShaders>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")