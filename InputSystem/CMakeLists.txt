add_library(InputSystem STATIC
    src/InputManager.cpp
)

target_include_directories(InputSystem 
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(InputSystem
    PUBLIC INPUTSYSTEM_STATIC
)

target_link_libraries(InputSystem PUBLIC
    user32
)