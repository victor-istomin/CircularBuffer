set(HEADER_LIST "${circularBuffer_SOURCE_DIR}/include/circularBuffer.hpp")

# IDEs should put the headers in a nice place
source_group(
    TREE   "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES  ${HEADER_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(circularBuffer INTERFACE ${circularBuffer_SOURCE_DIR}/include)

# All users of this library will need at least C++20 because of Ranges
target_compile_features(circularBuffer INTERFACE cxx_std_20)