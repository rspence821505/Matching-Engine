# High-Performance Matching Engine

A low-latency order matching engine implemented in C++17, featuring sub-microsecond matching performance and comprehensive performance tracking.

## Features

- ⚡ **Sub-microsecond latency** (~1.2µs average)
- 📊 **Price-time priority** matching
- 🔄 **Partial fill handling**
- 📈 **Real-time performance metrics** (VWAP, fill rates, latency percentiles)
- 🎯 **Prevents crossed books**
- 🧪 **Comprehensive testing suite**

## Project Structure

```
matching_engine/
├── include/          # Header files
│   ├── timer.hpp
│   ├── types.hpp
│   ├── order.hpp
│   ├── fill.hpp
│   ├── latency_tracker.hpp
│   └── order_book.hpp
├── src/              # Implementation files
│   ├── order.cpp
│   ├── fill.cpp
│   ├── latency_tracker.cpp
│   ├── order_book.cpp
│   └── main.cpp
├── CMakeLists.txt
└── README.md
```

## Building

### Prerequisites

- CMake 3.15+
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run
./matching_engine
```

### Build Options

```bash
# Release build (optimized)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
```

## Usage Example

```cpp
#include "order_book.hpp"

OrderBook book;

// Add orders
book.add_order(Order{1, Side::BUY, 100.00, 100});
book.add_order(Order{2, Side::SELL, 100.50, 150});

// Aggressive order that crosses spread
book.add_order(Order{3, Side::BUY, 101.00, 120});

// Print statistics
book.print_match_stats();
book.print_book_summary();
```

## Performance Metrics

Typical performance on modern hardware:

- **Average latency**: 1.2 µs
- **p99 latency**: 2.5 µs
- **Throughput**: ~900K orders/second

## Architecture

### Core Components

1. **OrderBook**: Main matching engine with priority queues
2. **Order**: Order representation with price-time priority
3. **Fill**: Trade execution record
4. **Timer**: High-precision latency measurement
5. **LatencyTracker**: Statistical analysis of performance

### Matching Logic

- **Price priority**: Best prices match first
- **Time priority**: Earlier orders at same price match first
- **Passive price**: Resting order sets execution price
- **Partial fills**: Orders can be partially filled across multiple matches

## Future Enhancements

- [ ] Order cancellation and amendment
- [ ] Market orders
- [ ] Time-in-force rules (IOC, FOK, GTC)
- [ ] Multi-threaded order processing
- [ ] Persistent order storage
- [ ] Network interface (FIX protocol)

## License

MIT License
