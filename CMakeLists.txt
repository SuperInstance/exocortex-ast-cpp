cmake_minimum_required(VERSION 3.14)
project(exocortex-ast-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library target
add_library(exocortex_ast INTERFACE)
target_include_directories(exocortex_ast INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Tests
add_executable(exocortex_ast_tests test_main.cpp)
target_link_libraries(exocortex_ast_tests PRIVATE exocortex_ast)

enable_testing()
add_test(NAME ast_tests COMMAND exocortex_ast_tests)
