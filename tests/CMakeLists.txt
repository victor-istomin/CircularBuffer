project(unit_tests VERSION 0.1 LANGUAGES CXX)

# doctest support
list(APPEND CMAKE_MODULE_PATH "${circularBuffer_SOURCE_DIR}/submodules/doctest.git/scripts/cmake")
include(doctest)

add_executable(unit_tests unit_tests.cpp)

target_link_libraries(unit_tests INTERFACE doctest)
target_link_libraries(unit_tests PRIVATE circularBuffer)

set(DOCTEST_INCLUDE_DIR ${circularBuffer_SOURCE_DIR}/submodules/doctest.git/doctest CACHE INTERNAL "Path to include folder for doctest")
target_include_directories(unit_tests PUBLIC ${DOCTEST_INCLUDE_DIR})
doctest_discover_tests(unit_tests)

# ranges support
option(WITH_CXX_20 "Use C++ 20" ON)
message(STATUS "[unit_tests] With C++ 20: ${WITH_CXX_20}")
if(WITH_CXX_20)
    target_compile_features(unit_tests PRIVATE cxx_std_20)
endif()

# test using -Wall + pedantic: trying to ensure acceptable library code quality
if (MSVC)
    target_compile_options(unit_tests PRIVATE /W4 /WX)
    target_compile_options(unit_tests PRIVATE /permissive-)
else()
    target_compile_options(unit_tests PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

# cppcheck support
option(WITH_CPPCHECK "Use cppcheck" OFF)
message(STATUS "[unit_tests] Use cppcheck: ${WITH_CPPCHECK}")
if(WITH_CPPCHECK)
    find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
    if(CMAKE_CXX_CPPCHECK)
        message(STATUS "cppcheck is found and will be used on the unit_tests")
        list(APPEND CMAKE_CXX_CPPCHECK "--enable=all" )
    else()
        message(WARNING "Could not find the program cppcheck")
    endif()
else()
    message(STATUS "Building with no cppcheck support")
endif()  # WITH_CPPCHECK

# clang-tidy support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


