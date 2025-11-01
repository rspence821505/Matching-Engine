#include "strategies.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

// Simulate market data for testing
class MarketDataSimulator {
private:
  std::mt19937 rng_;
  std::normal_distribution<> price_dist_;
  std::unordered_map<std::string, double> prices_;

public:
  MarketDataSimulator(unsigned seed = 42) : rng_(seed), price_dist_(0.0, 0.5) {

    // Initialize starting prices
    prices_["AAPL"] = 150.0;
    prices_["MSFT"] = 300.0;
    prices_["GOOGL"] = 140.0;
  }

  MarketDataSnapshot generate_snapshot(const std::string &symbol) {
    MarketDataSnapshot snapshot;
    snapshot.symbol = symbol;
    snapshot.timestamp = Clock::now();

    // Update price with random walk
    double &price = prices_[symbol];
    price += price_dist_(rng_);
    price = std::max(price, 10.0); // Floor at $10

    snapshot.last_price = price;
    snapshot.bid_price = price - 0.01;
    snapshot.ask_price = price + 0.01;
    snapshot.bid_size = 100;
    snapshot.ask_size = 100;
    snapshot.spread = 0.02;

    return snapshot;
  }

  void add_trend(const std::string &symbol, double trend_pct) {
    prices_[symbol] *= (1.0 + trend_pct / 100.0);
  }
};

int main() {
  std::cout << "╔══════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║           STRATEGY FRAMEWORK DEMONSTRATION               ║"
            << std::endl;
  std::cout << "╚══════════════════════════════════════════════════════════╝"
            << std::endl;

  // ========================================================================
  // PHASE 1: Create Strategy Configurations
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 1: Creating Strategy Configurations" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Momentum Strategy Config
  StrategyConfig momentum_config;
  momentum_config.name = "Momentum Trader";
  momentum_config.account_id = 1;
  momentum_config.symbols = {"AAPL", "MSFT"};
  momentum_config.max_position_size = 500;
  momentum_config.enabled = true;
  momentum_config.set_parameter("lookback_period", 20);
  momentum_config.set_parameter("entry_threshold", 2.0);
  momentum_config.set_parameter("exit_threshold", -0.5);
  momentum_config.set_parameter("take_profit", 5.0);
  momentum_config.set_parameter("stop_loss", 2.0);

  // Mean Reversion Strategy Config
  StrategyConfig mean_reversion_config;
  mean_reversion_config.name = "Mean Reversion";
  mean_reversion_config.account_id = 2;
  mean_reversion_config.symbols = {"GOOGL"};
  mean_reversion_config.max_position_size = 300;
  mean_reversion_config.enabled = true;
  mean_reversion_config.set_parameter("lookback_period", 20);
  mean_reversion_config.set_parameter("entry_std_devs", 2.0);
  mean_reversion_config.set_parameter("exit_std_devs", 0.5);
  mean_reversion_config.set_parameter("position_size_pct", 100.0);

  // Market Maker Strategy Config
  StrategyConfig market_maker_config;
  market_maker_config.name = "Market Maker";
  market_maker_config.account_id = 3;
  market_maker_config.symbols = {"AAPL"};
  market_maker_config.max_position_size = 1000;
  market_maker_config.enabled = false; // Start disabled for this demo
  market_maker_config.set_parameter("spread_bps", 10);
  market_maker_config.set_parameter("inventory_limit", 500);
  market_maker_config.set_parameter("quote_size", 100);
  market_maker_config.set_parameter("skew_factor", 0.1);

  // ========================================================================
  // PHASE 2: Instantiate Strategies
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 2: Instantiating Strategies" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::vector<std::unique_ptr<Strategy>> strategies;

  strategies.push_back(std::make_unique<MomentumStrategy>(momentum_config));
  strategies.push_back(
      std::make_unique<MeanReversionStrategy>(mean_reversion_config));
  strategies.push_back(
      std::make_unique<MarketMakerStrategy>(market_maker_config));

  // Initialize all strategies
  for (auto &strategy : strategies) {
    strategy->initialize();
  }

  // ========================================================================
  // PHASE 3: Simulate Market Data & Generate Signals
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 3: Market Data Simulation" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  MarketDataSimulator market_sim;
  const int num_ticks = 100;

  std::cout << "\nSimulating " << num_ticks << " market data ticks...\n"
            << std::endl;

  for (int i = 0; i < num_ticks; ++i) {
    // Add some trends to make it interesting
    if (i == 30) {
      std::cout << "\n>>> Adding upward trend to AAPL <<<\n" << std::endl;
      market_sim.add_trend("AAPL", 3.0); // +3%
    }
    if (i == 50) {
      std::cout << "\n>>> Adding downward trend to GOOGL <<<\n" << std::endl;
      market_sim.add_trend("GOOGL", -2.5); // -2.5%
    }
    if (i == 70) {
      std::cout << "\n>>> Adding upward trend to MSFT <<<\n" << std::endl;
      market_sim.add_trend("MSFT", 2.0); // +2%
    }

    // Generate market data for each symbol
    for (const auto &symbol : {"AAPL", "MSFT", "GOOGL"}) {
      MarketDataSnapshot snapshot = market_sim.generate_snapshot(symbol);

      // Feed to all strategies
      for (auto &strategy : strategies) {
        if (strategy->is_enabled()) {
          strategy->on_market_data(snapshot);
        }
      }
    }

    // Periodic signal generation (every 10 ticks)
    if (i % 10 == 0 && i > 20) {
      std::cout << "\n--- Tick " << i << " - Generating Signals ---"
                << std::endl;

      for (auto &strategy : strategies) {
        if (!strategy->is_enabled())
          continue;

        auto signals = strategy->generate_signals();

        if (!signals.empty()) {
          std::cout << "\n[" << strategy->get_name() << "] Generated "
                    << signals.size() << " signal(s):" << std::endl;

          for (const auto &signal : signals) {
            std::cout << "  " << signal.type_to_string() << " " << signal.symbol
                      << " | Confidence: " << std::fixed << std::setprecision(2)
                      << signal.confidence << " | Reason: " << signal.reason
                      << std::endl;

            // Convert signals to orders
            auto orders = strategy->signals_to_orders({signal});

            std::cout << "    → Generated " << orders.size() << " order(s)"
                      << std::endl;

            // Simulate fills (simplified)
            if (!orders.empty()) {
              for (const auto &order : orders) {
                Fill simulated_fill(order.id,
                                    order.id + 1000, // Simulated counterparty
                                    order.price > 0 ? order.price : 150.0,
                                    order.quantity);

                strategy->on_fill(simulated_fill);

                // Update strategy position
                int current_pos = strategy->get_position(signal.symbol);
                int new_pos = current_pos;

                if (order.side == Side::BUY) {
                  new_pos += order.quantity;
                } else {
                  new_pos -= order.quantity;
                }

                strategy->update_position(signal.symbol, new_pos);
              }
            }
          }
        }
      }
    }

    // Small delay for readability
    if ((i + 1) % 10 == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  // ========================================================================
  // PHASE 4: Strategy Performance Reports
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 4: Strategy Performance Summary" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  for (const auto &strategy : strategies) {
    strategy->print_summary();
    strategy->print_positions();
    std::cout << std::endl;
  }

  // ========================================================================
  // PHASE 5: Strategy Control Demonstration
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 5: Strategy Control Features" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::cout << "\nDemonstrating enable/disable functionality:" << std::endl;

  auto &momentum = strategies[0];
  std::cout << "\n"
            << momentum->get_name() << " is currently: "
            << (momentum->is_enabled() ? "ENABLED" : "DISABLED") << std::endl;

  momentum->disable();
  std::cout << "After disable(): "
            << (momentum->is_enabled() ? "ENABLED" : "DISABLED") << std::endl;

  momentum->enable();
  std::cout << "After enable(): "
            << (momentum->is_enabled() ? "ENABLED" : "DISABLED") << std::endl;

  // ========================================================================
  // PHASE 6: Individual Strategy Features
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 6: Strategy-Specific Features" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::cout << "\nMomentum Strategy - Price History:" << std::endl;
  const auto &aapl_history = momentum->get_price_history("AAPL");
  if (aapl_history.size() >= 5) {
    std::cout << "Last 5 prices for AAPL: ";
    for (size_t i = aapl_history.size() - 5; i < aapl_history.size(); ++i) {
      std::cout << std::fixed << std::setprecision(2) << aapl_history[i] << " ";
    }
    std::cout << std::endl;
  }

  std::cout << "\nMean Reversion Strategy - Current Positions:" << std::endl;
  auto &mean_rev = strategies[1];
  mean_rev->print_positions();

  // ========================================================================
  // Summary
  // ========================================================================
  std::cout << "\n╔══════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║                 DEMO COMPLETE                            ║"
            << std::endl;
  std::cout << "╚══════════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\nKey Features Demonstrated:" << std::endl;
  std::cout << "  ✓ Strategy configuration with parameters" << std::endl;
  std::cout << "  ✓ Multiple concurrent strategies" << std::endl;
  std::cout << "  ✓ Market data callbacks" << std::endl;
  std::cout << "  ✓ Signal generation logic" << std::endl;
  std::cout << "  ✓ Signal to order conversion" << std::endl;
  std::cout << "  ✓ Fill processing and position tracking" << std::endl;
  std::cout << "  ✓ Risk limit checks" << std::endl;
  std::cout << "  ✓ Performance statistics" << std::endl;
  std::cout << "  ✓ Enable/disable controls" << std::endl;
  std::cout << "  ✓ Price history and technical indicators" << std::endl;

  std::cout << "\nNext Steps:" << std::endl;
  std::cout << "  1. Integrate with OrderBook for real matching" << std::endl;
  std::cout << "  2. Link with PositionManager for P&L tracking" << std::endl;
  std::cout << "  3. Create TradingSimulator orchestration layer" << std::endl;
  std::cout << "  4. Add more strategy implementations" << std::endl;
  std::cout << "  5. Implement backtesting framework" << std::endl;

  return 0;
}