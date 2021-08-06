project(unit_tests VERSION 0.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${circularBuffer_SOURCE_DIR}/submodules/doctest.git/scripts/cmake")
include(doctest)

add_executable(unit_tests unit_tests.cpp)

target_link_libraries(unit_tests INTERFACE doctest)
target_link_libraries(unit_tests PRIVATE circularBuffer)

set(DOCTEST_INCLUDE_DIR ${circularBuffer_SOURCE_DIR}/submodules/doctest.git/doctest CACHE INTERNAL "Path to include folder for doctest")
target_include_directories(unit_tests PUBLIC ${DOCTEST_INCLUDE_DIR})

# test using -Wall + pedantic: trying to ensure acceptable library code quality
if (MSVC)
    target_compile_options(unit_tests PUBLIC /W4 /WX) 
    target_compile_options(unit_tests PUBLIC /permissive-)
else()
    target_compile_options(unit_tests PUBLIC -Wall -Wextra -pedantic -Werror)
endif()

# cppcheck support
find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if(CMAKE_CXX_CPPCHECK)
    message(STATUS "cppcheck is found and will be used on the unit_tests")
    list(APPEND CMAKE_CXX_CPPCHECK "--enable=all" )
else()
    message(WARNING "Could not find the program cppcheck")
endif()

doctest_discover_tests(unit_tests)
