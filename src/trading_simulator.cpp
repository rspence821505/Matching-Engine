// src/trading_simulator.cpp
#include "trading_simulator.hpp"
#include <iomanip>
#include <iostream>

TradingSimulator::TradingSimulator()
    : order_book_("SIM"), next_order_id_(1), is_running_(false) {}

// In trading_simulator.cpp or wherever you initialize
void TradingSimulator::setup() {
  // Register callback for automatic fill processing
  order_book_.get_fill_router().register_fill_callback(
      [this](const EnhancedFill &fill) {
        position_manager_.process_fill(fill.base_fill, fill.buy_account_id,
                                       fill.sell_account_id, fill.symbol);

        // Notify strategies
        for (auto &strategy : strategies_) {
          if (strategy->get_account_id() == fill.buy_account_id ||
              strategy->get_account_id() == fill.sell_account_id) {
            strategy->on_fill(fill.base_fill);
          }
        }
      });

  // Register self-trade notification
  order_book_.get_fill_router().register_self_trade_callback(
      [](int account_id, const Order &order1, const Order &order2) {
        std::cout << "⚠ Self-trade detected for account " << account_id
                  << " between orders " << order1.id << " and " << order2.id
                  << std::endl;
      });
}

void TradingSimulator::add_strategy(std::unique_ptr<Strategy> strategy) {
  if (!position_manager_.has_account(strategy->get_account_id())) {
    throw std::runtime_error("Account " +
                             std::to_string(strategy->get_account_id()) +
                             " must be created before adding strategy");
  }

  strategies_.push_back(std::move(strategy));
}

void TradingSimulator::create_account(int account_id, const std::string &name,
                                      double initial_cash) {
  position_manager_.create_account(account_id, name, initial_cash);
}

void TradingSimulator::run_simulation(size_t num_steps) {
  std::cout << "\n╔═══════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║          TRADING SIMULATOR STARTING                   ║"
            << std::endl;
  std::cout << "╚═══════════════════════════════════════════════════════╝"
            << std::endl;

  // Initialize all strategies
  for (auto &strategy : strategies_) {
    strategy->initialize();
  }

  is_running_ = true;

  // Main simulation loop
  for (size_t step = 0; step < num_steps && is_running_; ++step) {
    process_step();

    // Progress reporting
    if ((step + 1) % 100 == 0 || step == num_steps - 1) {
      std::cout << "\rProgress: " << (step + 1) << "/" << num_steps << " ("
                << std::fixed << std::setprecision(1)
                << ((step + 1) * 100.0 / num_steps) << "%)" << std::flush;
    }
  }

  std::cout << "\n\n✓ Simulation complete!" << std::endl;

  // Process any final fills
  process_fills();

  print_final_report();
}

void TradingSimulator::process_step() {
  // 1. Update market data for all strategies
  MarketDataSnapshot snapshot;
  snapshot.symbol = order_book_.get_symbol();
  snapshot.timestamp = Clock::now();

  // Get current market state
  auto best_bid = order_book_.get_best_bid();
  auto best_ask = order_book_.get_best_ask();

  if (best_bid) {
    snapshot.bid_price = best_bid->price;
    snapshot.bid_size = best_bid->remaining_qty;
  }

  if (best_ask) {
    snapshot.ask_price = best_ask->price;
    snapshot.ask_size = best_ask->remaining_qty;
  }

  if (best_bid && best_ask) {
    snapshot.last_price = (best_bid->price + best_ask->price) / 2.0;
  }

  auto spread = order_book_.get_spread();
  snapshot.spread = spread.value_or(0.0);

  // 2. Notify all strategies of market data
  for (auto &strategy : strategies_) {
    strategy->on_market_data(snapshot);
  }

  // 3. Generate signals from each strategy
  for (auto &strategy : strategies_) {
    if (!strategy->is_enabled())
      continue;

    auto signals = strategy->generate_signals();
    auto orders = strategy->signals_to_orders(signals);

    // Submit orders to order book
    for (auto &order : orders) {
      order.id = next_order_id_++;
      order_book_.add_order(order);
    }
  }

  // 4. Process any fills that occurred
  process_fills();

  // 5. Call timer callbacks
  for (auto &strategy : strategies_) {
    strategy->on_timer();
  }
}

void TradingSimulator::process_fills() {
  const auto &account_fills = order_book_.get_account_fills();
  static size_t last_processed = 0;

  // Process only new fills
  for (size_t i = last_processed; i < account_fills.size(); ++i) {
    const auto &af = account_fills[i];

    // Route fill to position manager
    position_manager_.process_fill(af.fill, af.buy_account_id,
                                   af.sell_account_id, af.symbol);

    // Notify strategies
    for (auto &strategy : strategies_) {
      if (strategy->get_account_id() == af.buy_account_id ||
          strategy->get_account_id() == af.sell_account_id) {
        strategy->on_fill(af.fill);
      }
    }
  }

  last_processed = account_fills.size();
}

void TradingSimulator::print_final_report() {
  std::cout << "\n╔═══════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║              SIMULATION FINAL REPORT                  ║"
            << std::endl;
  std::cout << "╚═══════════════════════════════════════════════════════╗"
            << std::endl;

  // Order book statistics
  std::cout << "\n=== Order Book Statistics ===" << std::endl;
  order_book_.print_match_stats();

  // Strategy performance
  std::cout << "\n=== Strategy Performance ===" << std::endl;
  for (const auto &strategy : strategies_) {
    strategy->print_summary();
  }

  // Position manager summary
  std::cout << "\n=== Account Summary ===" << std::endl;
  position_manager_.print_all_accounts();

  // Overall metrics
  std::cout << "\n=== Aggregate Metrics ===" << std::endl;
  std::cout << "Total Fills: " << order_book_.get_fills().size() << std::endl;
  std::cout << "Total Account Value: $" << std::fixed << std::setprecision(2)
            << position_manager_.get_total_account_value() << std::endl;
  std::cout << "Total P&L: $" << position_manager_.get_total_pnl() << std::endl;
}

void TradingSimulator::export_results(const std::string &filename) {
  position_manager_.export_all_accounts(filename);
  std::cout << "\n✓ Results exported to " << filename << std::endl;
}