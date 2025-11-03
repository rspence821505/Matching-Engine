# High-Performance Matching Engine

A production-grade limit-order book written in modern C++17. The engine delivers sub-microsecond latency, supports advanced order types, and includes full persistence, replay, and analytics tooling for realistic trading simulations.

## Highlights

- ⚡ **Ultra-low latency** price-time matching with deterministic outcomes
- 🧊 **Advanced order types**: limit, market, IOC/FOK/DAY, iceberg, stop-market, stop-limit
- 🛠️ **Complete order lifecycle**: add, amend (price/quantity), cancel with state tracking
- 🧾 **Account-aware fills**: every order carries ownership, enabling P&L attribution
- 💾 **Crash-safe persistence**: snapshots, event logs, checkpoint recovery, deterministic replay
- 📊 **Performance analytics**: latency histograms, fill-rate analysis, VWAP, risk metrics

## Project Layout

```
matching_engine/
├── include/
│   ├── account.hpp             # Account and P&L tracking
│   ├── event.hpp               # Serialized order events
│   ├── fill.hpp                # Trade execution records
│   ├── order.hpp               # Order model & constructors (account-aware)
│   ├── order_book.hpp          # Public order-book interface
│   ├── performance_metrics.hpp # Aggregated risk & return statistics
│   ├── position_manager.hpp    # Account-level position management
│   ├── replay_engine.hpp       # Event-driven backtesting/replay
│   ├── snapshot.hpp            # Persistence schema
│   ├── timer.hpp / types.hpp   # Shared utilities
│   └── ...
├── src/
│   ├── order_book.cpp            # High-level orchestration (add/cancel/amend)
│   ├── order_book_matching.cpp   # Matching loop, FOK/IOC handling, trade execution
│   ├── order_book_stops.cpp      # Stop-order trigger logic and reference pricing
│   ├── order_book_reporting.cpp  # Book introspection, depth views, latency stats
│   ├── order_book_persistence.cpp# Snapshots, checkpoints, CSV logging
│   ├── order.cpp / event.cpp / fill.cpp / snapshot.cpp / replay_engine.cpp
│   ├── position_manager.cpp      # Multi-account position & risk controls
│   ├── performance_metrics.cpp   # Sharpe, drawdown, Sortino, CSV export
│   └── main.cpp                  # Demo scenario showcasing account-aware matching
├── tests/                        # GoogleTest suite + performance benchmarks
├── build/                        # Generated build artifacts
├── CMakeLists.txt / tests/CMakeLists.txt
├── Makefile / build.sh
└── LICENSE
```

## Building

### Requirements

- C++17 toolchain (GCC 8+, Clang 7+, or MSVC 2019+)
- CMake 3.15+
- Ninja or Make (optional)
- GoogleTest (Homebrew package `googletest` on macOS) for the unit suite

### CMake Workflow (recommended)

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
./matching_engine            # run the demo scenario
```

Enable debug symbols and sanitizers:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
cmake --build . -j$(nproc)
```

### Makefile Shortcut

```bash
make          # release build (O3)
make run      # build + run demo
make debug    # debug symbols
make sanitize # debug + address/UB sanitizers
```

### Build Script

```bash
./build.sh    # one-command release build + run
```

## Running Tests

```bash
cmake --build build -j$(nproc) --target run_tests
./build/tests/run_tests
```

The suite covers:

- Order lifecycle, matching logic, iceberg refresh, stop triggers
- Persistence (snapshot, checkpoint, replay determinism)
- Position manager and account summary behaviour
- Performance metrics (Sharpe, drawdown, Sortino, CSV export) via a dual-mode benchmark harness

## Quick Start Example

```cpp
#include "order_book.hpp"

int main() {
  OrderBook book("XYZ");

  // Submit two accounts
  book.add_order(Order(1, 101, Side::BUY, 100.00, 100));               // acct 101
  book.add_order(Order(2, 202, Side::SELL, 100.00, 100));              // acct 202

  // Aggressive IOC market order for account 101
  book.add_order(Order(3, 101, Side::BUY, OrderType::MARKET, 150,
                       TimeInForce::IOC));

  book.print_top_of_book();
  book.print_account_fills();
}
```

## Performance & Observability

- High-resolution timers for insertion latency and matching statistics (p50/p95/p99)
- Market depth dumps (full table or compact view)
- Trade timeline, fill-rate analysis, VWAP reporting
- CSV exports for both event logs and aggregated performance metrics (risk/return analytics)

## License

This project is released under the [MIT License](LICENSE).
