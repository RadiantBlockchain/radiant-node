# Copyright (c) 2020 The Bitcoin developers

project(asert_testgen)

include(DoOrFail)
if (APPLE)
    find_program_or_fail(CSPLIT_EXECUTABLE gcsplit)
else()
    find_program_or_fail(CSPLIT_EXECUTABLE csplit)
endif()

# This project must use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gen_asert_test_vectors
    EXCLUDE_FROM_ALL
    gen_asert_test_vectors.cpp)

target_link_libraries(gen_asert_test_vectors common bitcoinconsensus server)

set(ASERT_TEST_VECTORS_OUTPUT "asert_test_vectors.txt")

add_custom_target(
    asert_test_vectors_big_runs_file
    BYPRODUCTS "${ASERT_TEST_VECTORS_OUTPUT}"
    COMMAND gen_asert_test_vectors > "${ASERT_TEST_VECTORS_OUTPUT}"
    DEPENDS gen_asert_test_vectors
    COMMENT "Running gen_asert_test_vectors to produce big runs file")

add_custom_target(
    asert_test_vectors_run_files
    BYPRODUCTS run01 run02 run03 run04 run05 run06 run07 run08 run09 run10
    COMMAND
        "${CSPLIT_EXECUTABLE}"
        "--prefix=run"
        "--suffix-format='%02d'"
        "${ASERT_TEST_VECTORS_OUTPUT}"
        "'/^## description:/'"
        "'{*}'"
    DEPENDS asert_test_vectors_big_runs_file
    COMMENT "Splitting output file into separate run files")

add_custom_target(
    asert_test_vectors_finalize
    COMMAND rm run00
    DEPENDS asert_test_vectors_run_files
    COMMENT "Finalizing run files")

add_custom_target(asert-testgen DEPENDS asert_test_vectors_finalize)
