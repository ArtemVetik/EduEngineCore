set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(TemporalAA WIN32 src/main.cpp)

target_sources(TemporalAA PRIVATE
    src/TemporalAADemo.cpp
    src/TemporalAADemoFactory.cpp
)

target_include_directories(TemporalAA
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(TemporalAA PRIVATE
    BaseDemo
)

add_custom_command(TARGET TemporalAA POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:TemporalAA>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:TemporalAA>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:TemporalAA>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")