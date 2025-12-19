set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(AsyncCompute WIN32 src/main.cpp)

target_sources(AsyncCompute PRIVATE
    src/AsyncComputeDemo.cpp
    src/AsyncComputeDemoFactory.cpp
)

target_include_directories(AsyncCompute
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(AsyncCompute PRIVATE
    BaseDemo
    InputSystem
)

add_custom_command(TARGET AsyncCompute POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:AsyncCompute>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:AsyncCompute>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")