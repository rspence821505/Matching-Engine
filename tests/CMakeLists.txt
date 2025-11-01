# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

enable_testing()

# Find GTest
find_package(GTest REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(TEST_SOURCES
    main.cpp
    test_order.cpp
    test_order_book_basic.cpp
    test_order_book_advanced.cpp
    test_matching_logic.cpp
    test_iceberg_orders.cpp
    test_stop_orders.cpp
    test_tif_policies.cpp
    test_persistence.cpp
    test_replay_engine.cpp
    test_account.cpp
    test_position_manager.cpp
    test_performance_benchmarks.cpp
)

add_executable(run_tests ${TEST_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/order.cpp
    ${PROJECT_SOURCE_DIR}/src/fill.cpp
    ${PROJECT_SOURCE_DIR}/src/latency_tracker.cpp
    ${PROJECT_SOURCE_DIR}/src/order_book.cpp
    ${PROJECT_SOURCE_DIR}/src/order_book_matching.cpp
    ${PROJECT_SOURCE_DIR}/src/order_book_stops.cpp
    ${PROJECT_SOURCE_DIR}/src/order_book_reporting.cpp
    ${PROJECT_SOURCE_DIR}/src/order_book_persistence.cpp
    ${PROJECT_SOURCE_DIR}/src/event.cpp
    ${PROJECT_SOURCE_DIR}/src/snapshot.cpp
    ${PROJECT_SOURCE_DIR}/src/replay_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/account.cpp
    ${PROJECT_SOURCE_DIR}/src/position_manager.cpp
)

# Link with explicit library paths for macOS
if(APPLE)
    # Find Homebrew prefix
    execute_process(
        COMMAND brew --prefix googletest
        OUTPUT_VARIABLE GTEST_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # Link against static libraries directly
    target_link_libraries(run_tests 
        ${GTEST_PREFIX}/lib/libgtest.a
        ${GTEST_PREFIX}/lib/libgtest_main.a
        pthread
    )
    target_include_directories(run_tests PRIVATE ${GTEST_PREFIX}/include)
else()
    target_link_libraries(run_tests 
        GTest::gtest
        GTest::gtest_main
    )
endif()

add_test(NAME AllTests COMMAND run_tests)
