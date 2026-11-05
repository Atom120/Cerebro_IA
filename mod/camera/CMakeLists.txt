# mod/camera/CMakeLists.txt - Dueño: Gonzalo Robles (Tarea G3)
cmake_minimum_required(VERSION 3.20)

add_library(aperture_runner_camera STATIC
    camera_feel.cpp
)

target_include_directories(aperture_runner_camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(aperture_runner_camera PUBLIC
    ar_contracts
)

if(MSVC)
    target_compile_options(aperture_runner_camera PRIVATE /W4 /WX /permissive-)
endif()

add_executable(test_camera
    tests/test_camera.cpp
)

target_link_libraries(test_camera PRIVATE
    aperture_runner_camera
    ar_contracts
)

if(MSVC)
    target_compile_options(test_camera PRIVATE /W4 /WX /permissive-)
endif()

add_test(NAME test_camera COMMAND test_camera)

# Si existe el modulo de cliente (mod/plugin/client/CMakeLists.txt, tambien de Gonzalo) y aun
# no ha sido incluido por mod/plugin/CMakeLists.txt, lo incorporamos al build y a CTest sin
# tocar archivos fuera de las carpetas de Gonzalo.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/client/CMakeLists.txt" AND NOT TARGET ar_client_plugin)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../plugin/client" "${CMAKE_CURRENT_BINARY_DIR}/client")
endif()
