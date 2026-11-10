# Extractor de Faith (A1-A4): Python puro. Solo registra sus pruebas en ctest.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME extractor_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_extractor.py)
    set_tests_properties(extractor_tests PROPERTIES ENVIRONMENT "PYTHONIOENCODING=utf-8")
endif()
