# High-Performance Order Matching Engine

A production-grade limit order book and matching engine implemented in C++17, featuring sub-microsecond latency, comprehensive order lifecycle management, and full crash recovery capabilities.

## Features

### Core Matching Engine

- ⚡ **Sub-microsecond matching latency** (~1.2µs average, <2.5µs p99)
- 📊 **Price-time priority** matching algorithm
- 🔄 **Partial fill handling** with automatic order book updates
- 🎯 **Crossed book prevention** with market depth validation
- 📈 **Multiple order types**: Limit, Market, Iceberg, Stop-Market, Stop-Limit
- ⏰ **Time-in-Force support**: GTC, IOC, FOK, DAY

### Order Lifecycle Management

- ✅ **Full order CRUD operations**: Add, Cancel, Amend
- 🧊 **Iceberg orders** with display quantity refresh
- 🛑 **Stop orders** with automatic triggering on price conditions
- 📋 **Order state tracking**: Pending, Active, Partially Filled, Filled, Cancelled

### Persistence & Recovery

- 💾 **Snapshot creation** with full order book state
- 🔄 **Event sourcing** with incremental replay
- 🚀 **Crash recovery** from checkpoint (snapshot + events)
- 📝 **CSV event logging** for deterministic replay
- ✅ **Validation framework** for replay correctness

### Performance Analytics

- 📊 **Real-time latency tracking** (p50, p95, p99, p99.9)
- 📈 **Volume-weighted average price (VWAP)** calculation
- 💹 **Fill rate analysis** and order statistics
- 🎯 **Market depth visualization** with multi-level book display

## Project Structure

```
matching_engine/
├── include/              # Header files
│   ├── event.hpp        # Order event definitions
│   ├── fill.hpp         # Trade execution records
│   ├── latency_tracker.hpp
│   ├── order.hpp        # Order types and state
│   ├── order_book.hpp   # Main matching engine
│   ├── replay_engine.hpp
│   ├── snapshot.hpp     # Persistence structures
│   ├── timer.hpp        # High-resolution timing
│   └── types.hpp        # Common type definitions
├── src/                 # Implementation files
│   ├── event.cpp        # Event serialization
│   ├── fill.cpp         # Fill record handling
│   ├── latency_tracker.cpp
│   ├── main.cpp         # Demo and testing
│   ├── order.cpp        # Order lifecycle logic
│   ├── order_book.cpp   # Core matching engine (~860 LOC)
│   ├── replay_engine.cpp
│   └── snapshot.cpp     # Persistence implementation
├── build/               # Build artifacts (generated)
├── CMakeLists.txt       # CMake build configuration
├── Makefile             # Alternative build system
├── build.sh             # Quick build script
├── README.md
└── LICENSE
```

## Building

### Prerequisites

- **C++17 compatible compiler**
  - GCC 8+
  - Clang 7+
  - MSVC 2019+
- **CMake 3.15+** (for CMake build)
- **Make** (for Makefile build)

### Build with CMake

```bash
# Create build directory
mkdir build && cd build

# Configure (Release mode)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -j$(nproc)

# Run
./matching_engine
```

### Build with Make

```bash
# Release build (default)
make

# Run
make run

# Debug build with symbols
make debug

# Debug build with sanitizers
make sanitize

# Clean
make clean
```

### Quick Build Script

```bash
# One-command build and run
./build.sh
```

### Build Options

**CMake:**

```bash
# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..

# Specify compiler
cmake -DCMAKE_CXX_COMPILER=clang++ ..
```

**Make:**

```bash
# Debug mode
make DEBUG=1

# With sanitizers
make SANITIZE=1

# Custom compiler
make CXX=g++

# Combined options
make DEBUG=1 SANITIZE=1 run
```

## Usage Examples

### Basic Order Matching

```cpp
#include "order_book.hpp"

OrderBook book;

// Add limit orders
book.add_order(Order{1, Side::BUY, 100.00, 100});   // Bid
book.add_order(Order{2, Side::SELL, 100.50, 150});  // Ask

// Add aggressive market order that crosses the spread
book.add_order(Order{3, Side::BUY, 101.00, 120});

// Print results
book.print_match_stats();
book.print_fills();
book.print_top_of_book();
```

### Advanced Order Types

```cpp
// Iceberg order (total 500 shares, display 100 at a time)
book.add_order(Order{4, Side::SELL, 100.25, 500, 100});

// Stop-loss order (sell if price drops to $98.00)
book.add_order(Order{5, Side::SELL, 98.00, 200, true});

// Stop-limit order (buy at $102.00 if price rises to $101.50)
book.add_order(Order{6, Side::BUY, 101.50, 102.00, 150});

// Immediate-or-Cancel (IOC) order
book.add_order(Order{7, Side::BUY, OrderType::MARKET, 100, TimeInForce::IOC});
```

### Order Lifecycle Management

```cpp
// Cancel an order
book.cancel_order(1);

// Amend price
book.amend_order(2, 100.25, std::nullopt);  // Change price

// Amend quantity
book.amend_order(2, std::nullopt, 200);     // Change quantity

// Amend both
book.amend_order(2, 100.30, 250);

// Query order status
book.print_order_status(2);
```

### Snapshot & Recovery

```cpp
// Enable event logging
book.enable_logging();

// ... process orders ...

// Create snapshot
book.save_snapshot("snapshot.txt");

// Create checkpoint (snapshot + incremental events)
book.save_checkpoint("snapshot.txt", "events.csv");

// Simulate crash and recovery
OrderBook recovered_book;
recovered_book.load_snapshot("snapshot.txt");

// Or recover from checkpoint
recovered_book.recover_from_checkpoint("snapshot.txt", "events.csv");
```

### Event Replay

```cpp
#include "replay_engine.hpp"

ReplayEngine replay;

// Load events from file
replay.load_from_file("events.csv");

// Instant replay (as fast as possible)
replay.replay_instant();

// Timed replay (2x speed)
replay.replay_timed(2.0);

// Step-by-step interactive replay
replay.replay_step_by_step();

// Manual control
while (replay.has_next_event()) {
    replay.replay_next_event();
    if (replay.get_current_index() % 100 == 0) {
        replay.get_book().print_top_of_book();
    }
}

// Validate replay correctness
replay.validate_against_original(original_fills);
```

### Performance Analysis

```cpp
// Market depth
book.print_market_depth(10);          // 10 levels
book.print_market_depth_compact();    // Compact view

// Statistics
book.print_match_stats();             // Volume, VWAP, latency
book.print_fill_rate_analysis();      // Fill rates
book.print_trade_timeline();          // Chronological fills
book.print_latency_stats();           // Latency distribution

// Pending stops
book.print_pending_stops();
```

## License

MIT License - See [LICENSE](LICENSE) file for details
