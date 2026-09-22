set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(EngineTests
    src/main.cpp
    src/TestAsserts.cpp
    src/ShaderCacheTests.cpp
    src/ShaderResourcesTests.cpp
    src/VariableSizeMemoryAllocatorTests.cpp
)

target_compile_definitions(EngineTests PRIVATE UNICODE _UNICODE)

target_link_libraries(EngineTests PRIVATE
    GraphicsHeaps
    EduBinding
    gtest
)

add_custom_command(TARGET EngineTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:EngineTests>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBS_ROOT}/lib/dxcompiler.dll"
        "${LIBS_ROOT}/lib/dxil.dll"
        "$<TARGET_FILE_DIR:EngineTests>"
)

add_test(
    NAME EngineTests
    COMMAND EngineTests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:EngineTests>"
)
