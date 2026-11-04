# mod/plugin/CMakeLists.txt - Dueño: Zarek
cmake_minimum_required(VERSION 3.20)

add_library(aperture_runner SHARED
    main.cpp
    server/server_plugin.cpp
    server/movement_hook.cpp
    server/server_trace_provider.cpp
    server/faith_driver.cpp
    client/client_movement_hook.cpp
)

target_link_libraries(aperture_runner PRIVATE ar_contracts faith_move)

target_include_directories(aperture_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)

set_target_properties(aperture_runner PROPERTIES
    OUTPUT_NAME "aperture_runner"
    PREFIX ""
)

if(MSVC)
    target_compile_options(aperture_runner PRIVATE /W4 /WX /permissive-)
    target_compile_definitions(aperture_runner PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Prueba unitaria autonoma: simula ser Source Engine y carga aperture_runner.dll
add_executable(test_plugin_load
    tests/test_plugin_load.cpp
    server/server_trace_provider.cpp
)
target_link_libraries(test_plugin_load PRIVATE ar_contracts)
target_include_directories(test_plugin_load PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)
if(MSVC)
    target_compile_options(test_plugin_load PRIVATE /W4 /WX)
    target_compile_definitions(test_plugin_load PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_dependencies(test_plugin_load aperture_runner)
add_test(NAME test_plugin_load COMMAND test_plugin_load "$<TARGET_FILE:aperture_runner>")
