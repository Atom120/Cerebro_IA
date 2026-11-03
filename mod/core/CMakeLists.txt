cmake_minimum_required(VERSION 3.20)
project(ApertureRunnerCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(aperture_runner_core STATIC
    src/shared_arena.cpp
    src/bridge.cpp
)
target_include_directories(aperture_runner_core PUBLIC include)
if(MSVC)
    target_compile_options(aperture_runner_core PRIVATE /W4 /WX)
endif()

add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE aperture_runner_core)

add_executable(ar_tool tools/ar_tool.cpp)
target_link_libraries(ar_tool PRIVATE aperture_runner_core)

enable_testing()
add_test(NAME test_core COMMAND test_core)
