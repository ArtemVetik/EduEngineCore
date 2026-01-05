set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/Libraries")

add_executable(MeshletConverter src/main.cpp)

target_sources(MeshletConverter PRIVATE src/model.cpp)

target_include_directories(MeshletConverter
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${LIBS_ROOT}/include
)

find_library(ASSIMP_LIB
    NAMES assimp-vc143-mt
    HINTS ${LIBS_ROOT}/lib
    NO_DEFAULT_PATH
)

if(NOT ASSIMP_LIB)
    message(FATAL_ERROR "Assimp library 'assimp-vc143-mt.lib' not found in ${LIBS_ROOT}/lib")
endif()

target_link_libraries(MeshletConverter
    PRIVATE
        meshoptimizer
        ${ASSIMP_LIB}
)

set(ASSIMP_DLL "${LIBS_ROOT}/lib/assimp-vc143-mt.dll")

if(NOT EXISTS "${ASSIMP_DLL}")
    message(FATAL_ERROR "Assimp DLL not found: ${ASSIMP_DLL}")
endif()

add_custom_command(TARGET MeshletConverter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${ASSIMP_DLL}"
        "$<TARGET_FILE_DIR:MeshletConverter>"
)