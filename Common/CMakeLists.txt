
add_library(Common STATIC
    src/SimpleMath.cpp
    src/DxException.cpp
    
    include/Asserts.h
    include/EngineTypes.h
    include/StringUtils.h
    include/DxException.h
    include/FileUtils.h

    include/SimpleMath.inl
)
target_include_directories(Common
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(Common
    PRIVATE UNICODE _UNICODE
    PUBLIC COMMON_STATIC
)