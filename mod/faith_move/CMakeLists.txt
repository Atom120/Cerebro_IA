# Faith movement library (P1-P5). Owner: Peña (mod/faith_move/). P1/P2 written by Sebas.
add_library(faith_move STATIC
    src/faith_movement.cpp
)
target_include_directories(faith_move PUBLIC include)
target_link_libraries(faith_move PUBLIC ar_contracts)
if(MSVC)
    target_compile_options(faith_move PRIVATE /W4 /WX)
endif()

add_executable(test_faith_move tests/test_faith_move.cpp)
target_link_libraries(test_faith_move PRIVATE faith_move)
if(MSVC)
    target_compile_options(test_faith_move PRIVATE /W4 /WX)
endif()
add_test(NAME test_faith_move COMMAND test_faith_move)
