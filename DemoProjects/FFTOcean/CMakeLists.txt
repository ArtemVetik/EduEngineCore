set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(FFTOcean WIN32 src/main.cpp)

target_sources(FFTOcean PRIVATE
    src/FFTOceanDemo.cpp
    src/FFTOceanDemoFactory.cpp
    src/MeshGenerator.cpp
    src/FFTOceanGUI.cpp
)

target_include_directories(FFTOcean
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(FFTOcean PRIVATE
    BaseDemo
)

add_custom_command(TARGET FFTOcean POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:FFTOcean>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:FFTOcean>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/RenderTechniques/assets"
        "$<TARGET_FILE_DIR:FFTOcean>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:FFTOcean>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")