set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(VolumetricCloudsDemo WIN32 src/main.cpp)

target_sources(VolumetricCloudsDemo PRIVATE
    src/VolumetricCloudsDemo.cpp
    src/VolumetricCloudsDemoFactory.cpp
)

target_include_directories(VolumetricCloudsDemo
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(VolumetricCloudsDemo PRIVATE
    BaseDemo
)

add_custom_command(TARGET VolumetricCloudsDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:VolumetricCloudsDemo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:VolumetricCloudsDemo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/RenderTechniques/assets"
        "$<TARGET_FILE_DIR:VolumetricCloudsDemo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:VolumetricCloudsDemo>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")