# Mirror's Edge read-only telemetry recorder and tests (B2).
# Owner: Benja (mod/tools/me_telemetry/).
add_library(me_telemetry STATIC
    me_telemetry.cpp
)
target_include_directories(me_telemetry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(me_telemetry PUBLIC ar_contracts)
if(MSVC)
    target_compile_options(me_telemetry PRIVATE /W4 /WX)
endif()

add_executable(me_telemetry_recorder main.cpp)
target_link_libraries(me_telemetry_recorder PRIVATE me_telemetry)
if(MSVC)
    target_compile_options(me_telemetry_recorder PRIVATE /W4 /WX)
endif()

add_executable(test_me_telemetry test_me_telemetry.cpp)
target_link_libraries(test_me_telemetry PRIVATE me_telemetry)
if(MSVC)
    target_compile_options(test_me_telemetry PRIVATE /W4 /WX)
endif()
add_test(NAME test_me_telemetry COMMAND test_me_telemetry)
