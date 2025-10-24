#include "order_book.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
  std::cout << "=== Order Book Persistence & Recovery ===" << std::endl
            << std::endl;

  // Phase 1: Build active order book
  std::cout << "PHASE 1: Building active order book" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  OrderBook book;
  book.enable_logging();

  // Add various orders
  book.add_order(Order{1, Side::BUY, 100.00, 200});
  book.add_order(Order{2, Side::BUY, 99.50, 300});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.50, 200});

  // Add iceberg
  book.add_order(Order{5, Side::SELL, 100.25, 500, 100});

  // Add stop orders
  book.add_order(Order{6, Side::SELL, 98.00, 100, true}); // Stop-loss
  book.add_order(Order{7, Side::BUY, 102.00, 120, true}); // Stop-buy

  // Execute some trades
  book.add_order(Order{8, Side::BUY, 101.00, 150});

  std::cout << "\nCurrent book state:" << std::endl;
  book.print_book_summary();
  book.print_pending_stops();
  book.print_match_stats();

  // Phase 2: Create snapshot
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 2: Creating snapshot" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  book.save_snapshot("orderbook_snapshot.txt");

  // Phase 3: Simulate system crash and recovery
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 3: Simulating system crash..." << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::cout << "CRASH! System going down..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Phase 4: Recovery
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 4: System restart - recovering from snapshot"
            << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  OrderBook recovered_book;
  recovered_book.load_snapshot("orderbook_snapshot.txt");

  std::cout << "\nRecovered book state:" << std::endl;
  recovered_book.print_book_summary();
  recovered_book.print_pending_stops();

  // Verify recovery
  std::cout << "\nVerifying recovery..." << std::endl;
  if (recovered_book.get_fills().size() == book.get_fills().size()) {
    std::cout << "Fill count matches!" << std::endl;
  } else {
    std::cout << "Fill count mismatch!" << std::endl;
  }

  // Phase 5: Continue trading after recovery
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 5: Continuing trading after recovery" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  recovered_book.add_order(Order{9, Side::SELL, 99.00, 100});
  recovered_book.print_fills();
  recovered_book.print_book_summary();

  // Phase 6: Checkpoint (snapshot + incremental events)
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 6: Creating checkpoint (snapshot + events)" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  book.save_checkpoint("checkpoint_snapshot.txt", "checkpoint_events.csv");

  return 0;
}