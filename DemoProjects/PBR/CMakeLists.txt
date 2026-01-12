set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(PBR WIN32 src/main.cpp)

target_sources(PBR PRIVATE
    src/PBRDemo.cpp
    src/PBRDemoFactory.cpp
)

target_include_directories(PBR
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(PBR PRIVATE
    BaseDemo
)

add_custom_command(TARGET PBR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:PBR>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:PBR>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:PBR>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")