project(unit_tests VERSION 0.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "/home/victor/CircularBuffer/submodules/doctest.git/scripts/cmake")
include(doctest)

add_executable(unit_tests unit_tests.cpp)

target_link_libraries(unit_tests INTERFACE doctest)
target_link_libraries(unit_tests PRIVATE circularBuffer)

set(DOCTEST_INCLUDE_DIR ${circularBuffer_SOURCE_DIR}/submodules/doctest.git/doctest CACHE INTERNAL "Path to include folder for doctest")
target_include_directories(unit_tests PUBLIC ${DOCTEST_INCLUDE_DIR})

# test using -Wall + pedantic: trying to ensure acceptable library code quality
if (MSVC)
    target_compile_options(unit_tests PUBLIC /W4 /WX)
else()
    target_compile_options(unit_tests PUBLIC -Wall -Wextra -pedantic -Werror)
endif()

doctest_discover_tests(unit_tests)
