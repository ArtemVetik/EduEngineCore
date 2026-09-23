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

# A fixed output directory, so that the asset target below can name it without
# depending on the executable, which would make the two targets depend on each other
set(TESTS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set_target_properties(EngineTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TESTS_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TESTS_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TESTS_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${TESTS_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${TESTS_OUTPUT_DIR}"
)

# A separate target, so that editing a test shader alone is enough for the tests to see the change
add_custom_target(EngineTestsAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "${TESTS_OUTPUT_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBS_ROOT}/lib/dxcompiler.dll"
        "${LIBS_ROOT}/lib/dxil.dll"
        "${TESTS_OUTPUT_DIR}"
    COMMENT "Copying assets for EngineTests"
)

add_dependencies(EngineTests EngineTestsAssets)

add_test(
    NAME EngineTests
    COMMAND EngineTests
    WORKING_DIRECTORY "${TESTS_OUTPUT_DIR}"
)
