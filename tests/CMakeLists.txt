cmake_minimum_required(VERSION 3.10)

# Test runner
add_executable(test_runner test_runner.c)
target_link_libraries(test_runner apex)

# Add tests
add_test(NAME basic_tests COMMAND test_runner)

# Example test files
configure_file(test_basic.md test_basic.md COPYONLY)

