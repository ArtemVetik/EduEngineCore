set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(VirtualShadowMaps WIN32 src/main.cpp)

target_sources(VirtualShadowMaps PRIVATE
    src/VirtualShadowMapsDemo.cpp
    src/VirtualShadowMapsDemoFactory.cpp
)

target_include_directories(VirtualShadowMaps
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(VirtualShadowMaps PRIVATE
    BaseDemo
)

add_custom_command(TARGET VirtualShadowMaps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:VirtualShadowMaps>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:VirtualShadowMaps>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/RenderTechniques/assets"
        "$<TARGET_FILE_DIR:VirtualShadowMaps>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:VirtualShadowMaps>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")