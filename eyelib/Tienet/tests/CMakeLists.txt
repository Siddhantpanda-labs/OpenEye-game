# Tienet Tests
cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Basic tests
add_executable(tienet_tests
    test_tienet.cpp
)

target_link_libraries(tienet_tests PRIVATE Tienet)

# Add tests
add_test(NAME tienet_tests COMMAND tienet_tests)

# Set output directory
set_target_properties(tienet_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
