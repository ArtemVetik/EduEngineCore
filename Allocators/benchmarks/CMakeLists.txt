add_executable(benchmarks src/main.cpp "include/Allocators.h")

target_include_directories(benchmarks PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(benchmarks PUBLIC
    Allocators
    Common
)