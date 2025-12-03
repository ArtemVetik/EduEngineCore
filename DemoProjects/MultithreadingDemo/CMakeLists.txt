set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(MultithreadingDemo WIN32 src/main.cpp)

target_sources(MultithreadingDemo PRIVATE
    src/MultithreadingDemo.cpp
    src/MultithreadingDemoFactory.cpp
)

target_include_directories(MultithreadingDemo
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(MultithreadingDemo PRIVATE
    BaseDemo
    InputSystem
)

add_custom_command(TARGET MultithreadingDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:MultithreadingDemo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:MultithreadingDemo>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")