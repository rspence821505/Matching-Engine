#pragma once

#include <cstddef>  // for size_t
#include <fstream>  // for std::ofstream (if exporting results)
#include <iostream> // for std::cout, std::endl (if printing reports)
#include <memory>   // for std::unique_ptr
#include <string>   // for std::string
#include <vector>   // for std::vector

// project headers
#include "order_book.hpp"       // for OrderBook
#include "position_manager.hpp" // for PositionManager
#include "strategy.hpp"         // for Strategy

// include/trading_simulator.hpp
class TradingSimulator {
private:
  OrderBook order_book_;
  PositionManager position_manager_;
  std::vector<std::unique_ptr<Strategy>> strategies_;

  int next_order_id_;
  bool is_running_;

public:
  TradingSimulator();

  // Setup
  void add_strategy(std::unique_ptr<Strategy> strategy);
  void create_account(int account_id, const std::string &name,
                      double initial_cash);

  // Execution
  void run_simulation(size_t num_steps);
  void process_step(); // One simulation tick

  // Results
  void print_final_report();
  void export_results(const std::string &filename);

  OrderBook &get_order_book() { return order_book_; }
};