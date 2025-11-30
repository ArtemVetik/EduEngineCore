add_library(PBR STATIC
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
    InputSystem
)