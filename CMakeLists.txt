cmake_minimum_required(VERSION 3.28)
project(Helix.Testing LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

helix_add_library(helix_testing ALIAS Testing)

add_library(helix_test_main STATIC Source/Main/TestMain.cpp)
add_library(Helix::TestMain ALIAS helix_test_main)
target_link_libraries(helix_test_main PUBLIC helix_testing)
helix_apply_options(helix_test_main)

add_library(helix_bench_main STATIC Source/Main/BenchMain.cpp)
add_library(Helix::BenchMain ALIAS helix_bench_main)
target_link_libraries(helix_bench_main PUBLIC helix_testing)
helix_apply_options(helix_bench_main)

helix_add_test_suite(helix_testing_tests DEPS Helix::Testing)
helix_add_benchmark_suite(helix_testing_bench DEPS Helix::Testing)
