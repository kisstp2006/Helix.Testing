cmake_minimum_required(VERSION 3.28)
project(Helix.Testing LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

helix_add_library(helix_testing ALIAS Testing)
if(HELIX_PLATFORM STREQUAL "linux")
    # The watchdog of the fuzz runner is a thread.
    find_package(Threads REQUIRED)
    target_link_libraries(helix_testing PUBLIC Threads::Threads)
endif()

add_library(helix_test_main STATIC Source/Main/TestMain.cpp)
add_library(Helix::TestMain ALIAS helix_test_main)
target_link_libraries(helix_test_main PUBLIC helix_testing)
helix_apply_options(helix_test_main)

add_library(helix_bench_main STATIC Source/Main/BenchMain.cpp)
add_library(Helix::BenchMain ALIAS helix_bench_main)
target_link_libraries(helix_bench_main PUBLIC helix_testing)
helix_apply_options(helix_bench_main)

add_library(helix_fuzz_main STATIC Source/Main/FuzzMain.cpp)
add_library(Helix::FuzzMain ALIAS helix_fuzz_main)
target_link_libraries(helix_fuzz_main PUBLIC helix_testing)
helix_apply_options(helix_fuzz_main)

helix_add_test_suite(helix_testing_tests DEPS Helix::Testing)
helix_add_fuzz_suite(helix_testing_fuzz DEPS Helix::Testing)
helix_add_benchmark_suite(helix_testing_bench DEPS Helix::Testing)
