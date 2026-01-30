set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(Sponza WIN32 src/main.cpp)

target_sources(Sponza PRIVATE
    src/SponzaDemo.cpp
    src/SponzaDemoFactory.cpp
)

target_include_directories(Sponza
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(Sponza PRIVATE
    BaseDemo
)

add_custom_command(TARGET Sponza POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:Sponza>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/DemoProjects/BaseDemo/assets"
        "$<TARGET_FILE_DIR:Sponza>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/RenderTechniques/assets"
        "$<TARGET_FILE_DIR:Sponza>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:Sponza>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")