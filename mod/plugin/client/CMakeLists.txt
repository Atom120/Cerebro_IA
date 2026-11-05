# mod/plugin/client/CMakeLists.txt - Dueño: Gonzalo Robles (Tareas G1, G2, G4)
cmake_minimum_required(VERSION 3.20)

add_library(ar_client_plugin STATIC
    client_prediction_hook.cpp
    render/faith_render.cpp
)

target_include_directories(ar_client_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ar_client_plugin PUBLIC
    ar_contracts
)

if(MSVC)
    target_compile_options(ar_client_plugin PRIVATE /W4 /WX /permissive-)
    target_compile_definitions(ar_client_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(test_client_hooks
    tests/test_client_hooks.cpp
)

target_link_libraries(test_client_hooks PRIVATE
    ar_client_plugin
    ar_contracts
)

if(MSVC)
    target_compile_options(test_client_hooks PRIVATE /W4 /WX /permissive-)
    target_compile_definitions(test_client_hooks PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_test(NAME test_client_hooks COMMAND test_client_hooks)
