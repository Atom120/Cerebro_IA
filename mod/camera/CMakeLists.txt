# Mirror's Edge style camera (G3). Owner: Gonzalo (mod/camera/). Written by Sebas.
add_library(faith_camera STATIC
    src/faith_camera.cpp
)
target_include_directories(faith_camera PUBLIC include)
target_link_libraries(faith_camera PUBLIC ar_contracts)
if(MSVC)
    target_compile_options(faith_camera PRIVATE /W4 /WX)
endif()

add_executable(test_camera tests/test_camera.cpp)
target_link_libraries(test_camera PRIVATE faith_camera)
if(MSVC)
    target_compile_options(test_camera PRIVATE /W4 /WX)
endif()
add_test(NAME test_camera COMMAND test_camera)
