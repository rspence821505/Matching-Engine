
#include "order_book.hpp"
#include "replay_engine.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Trade Replay System ===" << std::endl << std::endl;

  // Phase 1: Record session
  std::cout << "PHASE 1: Recording trading session..." << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  OrderBook book;
  book.enable_logging(); // Enable event logging

  // Execute some trades
  book.add_order(Order{1, Side::BUY, 100.00, 200});
  book.add_order(Order{2, Side::BUY, 99.75, 300});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.75, 200});

  // Add iceberg
  book.add_order(Order{5, Side::SELL, 100.50, 500, 100});

  // Aggressive order
  book.add_order(Order{6, Side::BUY, 101.00, 250});

  // Cancel and amend
  book.cancel_order(3);
  book.amend_order(4, 100.50, std::nullopt);

  // More trading
  book.add_order(Order{7, Side::BUY, OrderType::MARKET, 100});

  std::cout << "\n";
  book.print_match_stats();

  // Save original fills for validation
  auto original_fills = book.get_fills();

  // Save events to file
  book.save_events("trading_session.csv");

  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 2: Replaying trading session..." << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Phase 2: Replay instant
  ReplayEngine replay;
  replay.load_from_file("trading_session.csv");
  replay.replay_instant();

  // Validate
  replay.validate_against_original(original_fills);

  // Phase 3: Timed replay
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 3: Timed replay at 1000x speed..." << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  ReplayEngine timed_replay;
  timed_replay.load_from_file("trading_session.csv");
  timed_replay.replay_timed(1000.0);

  return 0;
}
