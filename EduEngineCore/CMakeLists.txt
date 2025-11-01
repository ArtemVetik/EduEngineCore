set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

file(GLOB_RECURSE MAIN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)
file(GLOB_RECURSE MAIN_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
)

add_executable(EduEngineCore WIN32
    ${MAIN_SOURCES}
    ${MAIN_HEADERS}
)

target_compile_definitions(EduEngineCore PRIVATE UNICODE _UNICODE)

target_include_directories(EduEngineCore PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(EduEngineCore PRIVATE
    BaseDemo
    PBR
    InputSystem
)

add_custom_command(TARGET EduEngineCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/Shaders"
        "$<TARGET_FILE_DIR:EduEngineCore>/Shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/Textures"
        "$<TARGET_FILE_DIR:EduEngineCore>/Textures"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/Models"
        "$<TARGET_FILE_DIR:EduEngineCore>/Models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBS_ROOT}/lib"
        "$<TARGET_FILE_DIR:EduEngineCore>"
	COMMAND ${CMAKE_COMMAND} -E copy
		"$<TARGET_FILE:Graphics>"
		"$<TARGET_FILE_DIR:EduEngineCore>"
)

message(STATUS "Build with: mkdir build && cd build && cmake .. && cmake --build .")
