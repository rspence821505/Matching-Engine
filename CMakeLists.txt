cmake_minimum_required(VERSION 3.15)
project(MatchingEngine VERSION 1.0 LANGUAGES CXX)

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ==============================================================================
# COMPILER FLAGS
# ==============================================================================

# Base compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Release flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# ==============================================================================
# INCLUDE DIRECTORIES
# ==============================================================================

include_directories(${PROJECT_SOURCE_DIR}/include)

# ==============================================================================
# SOURCE FILES
# ==============================================================================

# Core library sources (shared between executables)
set(LIBRARY_SOURCES
    src/order.cpp
    src/fill.cpp
    src/latency_tracker.cpp
    src/order_book.cpp
    src/order_book_matching.cpp
    src/order_book_stops.cpp
    src/order_book_reporting.cpp
    src/order_book_persistence.cpp
    src/event.cpp
    src/performance_metrics.cpp
    src/snapshot.cpp
    src/replay_engine.cpp
    src/position_manager.cpp
    src/account.cpp
    src/strategy.cpp
    src/strategies.cpp
    src/trading_simulator.cpp
    src/market_data_generator.cpp
)

# Main application source
set(MAIN_SOURCE src/main.cpp)

# Demo application source
set(DEMO_SOURCE examples/simulator_demo.cpp)

# ==============================================================================
# BUILD OPTIONS
# ==============================================================================

option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_DEMOS "Build demo applications" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# ==============================================================================
# LIBRARY TARGET (Optional - for better organization)
# ==============================================================================

if(BUILD_SHARED_LIBS)
    add_library(matching_engine_lib SHARED ${LIBRARY_SOURCES})
    set_target_properties(matching_engine_lib PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
else()
    add_library(matching_engine_lib STATIC ${LIBRARY_SOURCES})
endif()

# ==============================================================================
# EXECUTABLE TARGETS
# ==============================================================================

# Main matching engine executable
add_executable(matching_engine ${MAIN_SOURCE})
target_link_libraries(matching_engine PRIVATE matching_engine_lib)

# Simulator demo executable
if(BUILD_DEMOS)
    add_executable(simulator_demo ${DEMO_SOURCE})
    target_link_libraries(simulator_demo PRIVATE matching_engine_lib)
    
    message(STATUS "Building demo applications")
endif()

# ==============================================================================
# COMPILER OPTIONS PER TARGET
# ==============================================================================

# Apply sanitizers if enabled
if(ENABLE_SANITIZERS)
    set(SANITIZER_FLAGS -fsanitize=address,undefined)
    
    target_compile_options(matching_engine PRIVATE ${SANITIZER_FLAGS})
    target_link_options(matching_engine PRIVATE ${SANITIZER_FLAGS})
    
    if(BUILD_DEMOS)
        target_compile_options(simulator_demo PRIVATE ${SANITIZER_FLAGS})
        target_link_options(simulator_demo PRIVATE ${SANITIZER_FLAGS})
    endif()
    
    message(STATUS "Sanitizers enabled")
endif()

# ==============================================================================
# TESTING
# ==============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Building test suite")
endif()

# ==============================================================================
# INSTALLATION
# ==============================================================================

# Install executables
install(TARGETS matching_engine 
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

if(BUILD_DEMOS)
    install(TARGETS simulator_demo
        RUNTIME DESTINATION bin
        COMPONENT demos
    )
endif()

# Install library (if needed for linking)
install(TARGETS matching_engine_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT libraries
)

# Install headers (optional - for external projects)
install(DIRECTORY include/
    DESTINATION include/matching_engine
    FILES_MATCHING PATTERN "*.hpp"
    COMPONENT development
)

# ==============================================================================
# CUSTOM TARGETS
# ==============================================================================

# Run matching engine
add_custom_target(run
    COMMAND matching_engine
    DEPENDS matching_engine
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
    COMMENT "Running matching engine..."
)

# Run simulator demo
if(BUILD_DEMOS)
    add_custom_target(demo
        COMMAND simulator_demo
        DEPENDS simulator_demo
        WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
        COMMENT "Running trading simulator demo..."
    )
    
    # Run demo with specific mode
    add_custom_target(demo-momentum
        COMMAND simulator_demo 1
        DEPENDS simulator_demo
        COMMENT "Running momentum vs mean reversion demo..."
    )
    
    add_custom_target(demo-backtest
        COMMAND simulator_demo 2
        DEPENDS simulator_demo
        COMMENT "Running simple backtest demo..."
    )
    
    add_custom_target(demo-all
        COMMAND simulator_demo 3
        DEPENDS simulator_demo
        COMMENT "Running all demos..."
    )
endif()

# Clean all builds
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Removing all build files..."
)

# ==============================================================================
# PACKAGING (Optional - for distribution)
# ==============================================================================

set(CPACK_PACKAGE_NAME "MatchingEngine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-Performance Trading Simulator and Matching Engine")
set(CPACK_PACKAGE_VENDOR "Your Name")

include(CPack)

# ==============================================================================
# INFORMATION SUMMARY
# ==============================================================================

message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════")
message(STATUS "  Matching Engine & Trading Simulator Configuration")
message(STATUS "═══════════════════════════════════════════════════════")
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  Build Type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:        C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:            ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Compiler Flags:      ${CMAKE_CXX_FLAGS}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Sanitizers:          ${ENABLE_SANITIZERS}")
message(STATUS "  Tests:               ${BUILD_TESTS}")
message(STATUS "  Demos:               ${BUILD_DEMOS}")
message(STATUS "  Shared Libraries:    ${BUILD_SHARED_LIBS}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  matching_engine      - Main matching engine")
if(BUILD_DEMOS)
    message(STATUS "  simulator_demo       - Trading simulator demo")
endif()
if(BUILD_TESTS)
    message(STATUS "  run_tests            - Unit test suite")
endif()
message(STATUS "")
message(STATUS "Custom Targets:")
message(STATUS "  make run             - Run matching engine")
if(BUILD_DEMOS)
    message(STATUS "  make demo            - Run all simulator demos")
    message(STATUS "  make demo-momentum   - Run momentum vs mean reversion")
    message(STATUS "  make demo-backtest   - Run simple backtest")
    message(STATUS "  make demo-all        - Run all demos sequentially")
endif()
message(STATUS "")
message(STATUS "Installation:")
message(STATUS "  Prefix:              ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Binary directory:    ${CMAKE_INSTALL_PREFIX}/bin")
message(STATUS "  Library directory:   ${CMAKE_INSTALL_PREFIX}/lib")
message(STATUS "  Include directory:   ${CMAKE_INSTALL_PREFIX}/include")
message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════")
message(STATUS "")

# ==============================================================================
# COMPILER-SPECIFIC WARNINGS
# ==============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(matching_engine_lib PRIVATE
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(matching_engine_lib PRIVATE
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(matching_engine_lib PRIVATE
        /W4
        /wd4100  # Unreferenced formal parameter
    )
endif()

# ==============================================================================
# VERIFICATION
# ==============================================================================

# Check if all required source files exist
set(REQUIRED_SOURCES
    src/trading_simulator.cpp
    src/strategy.cpp
    src/strategies.cpp
    src/account.cpp
    examples/simulator_demo.cpp
)

foreach(source ${REQUIRED_SOURCES})
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${source}")
        message(WARNING "Required source file missing: ${source}")
    endif()
endforeach()

# ==============================================================================
# DEVELOPMENT HELPERS
# ==============================================================================

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Print all variables (for debugging CMake configuration)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    # Uncomment to debug CMake variables
    # get_cmake_property(_variableNames VARIABLES)
    # foreach(_variableName ${_variableNames})
    #     message(STATUS "${_variableName}=${${_variableName}}")
    # endforeach()
endif()
