#!/bin/bash

# Clean previous build
rm -rf build

# Create build directory
mkdir build
cd build

# Configure and build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# Run if build successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful! Running matching engine..."
    echo ""
    ./matching_engine
fi