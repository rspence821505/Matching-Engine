cmake_minimum_required(VERSION 3.15)
project(MatchingEngine VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/order.cpp
    src/fill.cpp
    src/latency_tracker.cpp
    src/order_book.cpp
    src/main.cpp
)

# Executable
add_executable(matching_engine ${SOURCES})

# Optional: Enable sanitizers for debug builds
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(matching_engine PRIVATE -fsanitize=address,undefined)
    target_link_options(matching_engine PRIVATE -fsanitize=address,undefined)
endif()

# Installation rules
install(TARGETS matching_engine DESTINATION bin)