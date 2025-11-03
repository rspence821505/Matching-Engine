// examples/simulator_demo.cpp
#include "strategies.hpp"
#include "trading_simulator.hpp"
#include <iostream>
#include <memory>

void run_momentum_vs_mean_reversion_demo() {
  std::cout << "\n╔═══════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║     DEMO: Momentum vs Mean Reversion Strategies       ║"
            << std::endl;
  std::cout << "╚═══════════════════════════════════════════════════════╝"
            << std::endl;

  TradingSimulator sim;

  // Create accounts
  sim.create_account(1001, "Momentum Trader", 1000000.0);
  sim.create_account(1002, "Mean Reversion Trader", 1000000.0);
  sim.create_account(1003, "Market Maker", 2000000.0);

  // Configure momentum strategy
  StrategyConfig momentum_config;
  momentum_config.name = "Trend Follower";
  momentum_config.account_id = 1001;
  momentum_config.symbols = {"AAPL"};
  momentum_config.max_position_size = 500;
  momentum_config.enabled = true;
  momentum_config.set_parameter("lookback_period", 20.0);
  momentum_config.set_parameter("entry_threshold", 2.0);
  momentum_config.set_parameter("exit_threshold", -0.5);
  momentum_config.set_parameter("take_profit", 5.0);
  momentum_config.set_parameter("stop_loss", 2.0);

  auto momentum_strategy = std::make_unique<MomentumStrategy>(momentum_config);
  sim.add_strategy(std::move(momentum_strategy));

  // Configure mean reversion strategy
  StrategyConfig mean_rev_config;
  mean_rev_config.name = "Mean Reversion";
  mean_rev_config.account_id = 1002;
  mean_rev_config.symbols = {"AAPL"};
  mean_rev_config.max_position_size = 500;
  mean_rev_config.enabled = true;
  mean_rev_config.set_parameter("lookback_period", 20.0);
  mean_rev_config.set_parameter("entry_std_devs", 2.0);
  mean_rev_config.set_parameter("exit_std_devs", 0.5);
  mean_rev_config.set_parameter("position_size_pct", 100.0);

  auto mean_rev_strategy =
      std::make_unique<MeanReversionStrategy>(mean_rev_config);
  sim.add_strategy(std::move(mean_rev_strategy));

  // Configure market maker strategy
  StrategyConfig mm_config;
  mm_config.name = "Market Maker";
  mm_config.account_id = 1003;
  mm_config.symbols = {"AAPL"};
  mm_config.max_position_size = 1000;
  mm_config.enabled = true;
  mm_config.set_parameter("spread_bps", 10.0);
  mm_config.set_parameter("inventory_limit", 500.0);
  mm_config.set_parameter("quote_size", 100.0);
  mm_config.set_parameter("skew_factor", 0.1);

  auto mm_strategy = std::make_unique<MarketMakerStrategy>(mm_config);
  sim.add_strategy(std::move(mm_strategy));

  // Run simulation
  std::cout << "\nStarting simulation with 1000 steps..." << std::endl;
  sim.run_simulation(1000);

  // Export results
  sim.export_results("simulation_results.txt");
}

void run_simple_backtest_demo() {
  std::cout << "\n╔═══════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║          DEMO: Simple Backtesting Example             ║"
            << std::endl;
  std::cout << "╚═══════════════════════════════════════════════════════╝"
            << std::endl;

  TradingSimulator sim;

  // Single strategy test
  sim.create_account(2001, "Backtest Strategy", 500000.0);

  StrategyConfig config;
  config.name = "Simple Momentum";
  config.account_id = 2001;
  config.symbols = {"AAPL"};
  config.max_position_size = 1000;
  config.enabled = true;
  config.set_parameter("lookback_period", 10.0);
  config.set_parameter("entry_threshold", 1.5);

  auto strategy = std::make_unique<MomentumStrategy>(config);
  sim.add_strategy(std::move(strategy));

  std::cout << "\nRunning quick backtest (200 steps)..." << std::endl;
  sim.run_simulation(200);
}

int main(int argc, char **argv) {
  std::cout << "\n╔═══════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║        TRADING SIMULATOR DEMONSTRATION                ║"
            << std::endl;
  std::cout << "╚═══════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\nAvailable demos:" << std::endl;
  std::cout << "  1. Momentum vs Mean Reversion (Multi-Strategy)" << std::endl;
  std::cout << "  2. Simple Backtest (Single Strategy)" << std::endl;
  std::cout << "  3. Run Both" << std::endl;

  int choice = 3;
  if (argc > 1) {
    choice = std::atoi(argv[1]);
  } else {
    std::cout << "\nSelect demo (1-3) [default=3]: ";
    std::cin >> choice;
  }

  switch (choice) {
  case 1:
    run_momentum_vs_mean_reversion_demo();
    break;
  case 2:
    run_simple_backtest_demo();
    break;
  case 3:
    run_simple_backtest_demo();
    std::cout << "\n\n";
    run_momentum_vs_mean_reversion_demo();
    break;
  default:
    std::cerr << "Invalid choice!" << std::endl;
    return 1;
  }

  std::cout << "\n╔═══════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║              DEMONSTRATION COMPLETE                   ║"
            << std::endl;
  std::cout << "╚═══════════════════════════════════════════════════════╝"
            << std::endl;

  return 0;
}