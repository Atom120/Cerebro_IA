# ApertureRunner mod: Mirror's Edge movement inside Portal 1 (testchmb_a_00).
# Owner: Sebas. Build for Win32/x86 (Portal is 32-bit):
#   cmake -S mod -B build/x86 -A Win32
#   cmake --build build/x86 --config Release
#   ctest --test-dir build/x86 -C Release --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(ApertureRunnerMod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING "Configuring for 64-bit: tests run, but Portal only loads the plugin from a Win32 build (-A Win32).")
endif()

add_subdirectory(core)

# Shared contracts (header-only). Every module links this target.
add_library(ar_contracts INTERFACE)
target_include_directories(ar_contracts INTERFACE include)
target_link_libraries(ar_contracts INTERFACE aperture_runner_core)

add_executable(test_contracts tests/test_contracts.cpp)
target_link_libraries(test_contracts PRIVATE ar_contracts)
target_compile_definitions(test_contracts PRIVATE
    AR_ASSET_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/../docs/contratos/asset_manifest.json")
if(MSVC)
    target_compile_options(test_contracts PRIVATE /W4 /WX)
endif()
add_test(NAME test_contracts COMMAND test_contracts)

# Each owner adds a CMakeLists.txt in their own folder; it is picked up here without editing
# this file. Link ar_contracts and register tests with add_test.
foreach(module faith_move camera plugin tools/me_telemetry tools/extractor)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module}/CMakeLists.txt")
        add_subdirectory(${module})
    endif()
endforeach()
