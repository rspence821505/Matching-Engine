#include "fill_router.hpp"
#include "market_data_generator.hpp"
#include "strategies.hpp"
#include "trading_simulator.hpp"

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <utility>

namespace {

std::string liquidity_flag_to_string(EnhancedFill::LiquidityFlag flag) {
  switch (flag) {
  case EnhancedFill::LiquidityFlag::MAKER:
    return "MAKER";
  case EnhancedFill::LiquidityFlag::TAKER:
    return "TAKER";
  case EnhancedFill::LiquidityFlag::MAKER_MAKER:
    return "MAKER_MAKER";
  }
  return "UNKNOWN";
}

void create_core_accounts(TradingSimulator &sim) {
  std::vector<std::tuple<int, std::string, double>> account_specs = {
      {6001, "Maker-Buy-L1", 1'000'000.0},
      {6002, "Maker-Cross-L2", 1'000'000.0},
      {6003, "Maker-Ladder-L3", 1'000'000.0},
      {6004, "Maker-Ladder-L4", 1'000'000.0},
      {6005, "Maker-Ask-Overflow", 1'000'000.0},
      {7001, "Aggressive-Buyer", 750'000.0},
      {7002, "Aggressive-Seller", 750'000.0},
      {8001, "Momentum Strategy", 500'000.0},
      {8002, "MeanReversion Strategy", 500'000.0}};

  for (const auto &[id, name, cash] : account_specs) {
    sim.create_account(id, name, cash);
  }
}

StrategyConfig build_momentum_config() {
  StrategyConfig cfg;
  cfg.name = "Momentum-Alpha";
  cfg.account_id = 8001;
  cfg.symbols = {"SIM"};
  cfg.max_position_size = 1'000;
  cfg.set_parameter("lookback_period", 25.0);
  cfg.set_parameter("entry_threshold", 1.8);
  cfg.set_parameter("exit_threshold", -0.6);
  cfg.set_parameter("take_profit", 4.5);
  cfg.set_parameter("stop_loss", 1.8);
  return cfg;
}

StrategyConfig build_mean_reversion_config() {
  StrategyConfig cfg;
  cfg.name = "MeanReversion-Beta";
  cfg.account_id = 8002;
  cfg.symbols = {"SIM"};
  cfg.max_position_size = 800;
  cfg.set_parameter("lookback_period", 30.0);
  cfg.set_parameter("entry_std_devs", 2.5);
  cfg.set_parameter("exit_std_devs", 0.75);
  cfg.set_parameter("position_size_pct", 80.0);
  return cfg;
}

} // namespace

int main() {
  std::cout << "╔════════════════════════════════════════════════════════════╗\n"
            << "║      Matching Engine – Full System Integration Demo        ║\n"
            << "╚════════════════════════════════════════════════════════════╝\n"
            << std::endl;

  TradingSimulator simulator;
  simulator.setup();

  create_core_accounts(simulator);

  auto &book = simulator.get_order_book();
  book.set_symbol("SIM");
  book.enable_logging();

  FillRouter &router = book.get_fill_router();
  router.set_self_trade_prevention(true);
  router.set_fee_schedule(/* maker */ 0.00005, /* taker */ 0.0002);

  router.register_fill_callback(
      [printed = 0](const EnhancedFill &fill) mutable {
        if (printed < 12 || printed % 25 == 0) {
          std::cout << std::fixed << std::setprecision(2);
          std::cout << "  • Fill #" << fill.fill_id << " "
                    << fill.base_fill.quantity << " @ $" << fill.base_fill.price
                    << " | BuyAcct=" << fill.buy_account_id
                    << " SellAcct=" << fill.sell_account_id
                    << " Liquidity="
                    << liquidity_flag_to_string(fill.liquidity_flag)
                    << " Fees(B=" << fill.buyer_fee
                    << ", S=" << fill.seller_fee << ")\n";
        } else if (printed == 12) {
          std::cout << "  • … additional fills suppressed for brevity …\n";
        }
        ++printed;
      });

  router.register_self_trade_callback(
      [](int account_id, const Order &o1, const Order &o2) {
        std::cout << "  ⚠ Self-trade prevented for account " << account_id
                  << " between orders " << o1.id << " and " << o2.id << "\n";
      });

  auto momentum = std::make_unique<MomentumStrategy>(build_momentum_config());
  momentum->initialize();
  simulator.add_strategy(std::move(momentum));

  auto mean_reversion =
      std::make_unique<MeanReversionStrategy>(build_mean_reversion_config());
  mean_reversion->initialize();
  simulator.add_strategy(std::move(mean_reversion));

  MarketDataGenerator::Config gen_cfg;
  gen_cfg.symbol = "SIM";
  gen_cfg.start_price = 100.0;
  gen_cfg.volatility = 0.8;
  gen_cfg.spread = 0.05;
  gen_cfg.depth_levels = 4;
  gen_cfg.maker_buy_account = 6001;
  gen_cfg.maker_sell_account = 6002;
  gen_cfg.taker_buy_account = 7001;
  gen_cfg.taker_sell_account = 7002;

  MarketDataGenerator generator(gen_cfg);
  generator.register_callback([print_count = 0](const MarketDataSnapshot &snap) mutable {
    if (print_count < 5 || (print_count % 25 == 0 && print_count < 150)) {
      std::cout << "  ≈ Snapshot " << snap.symbol << " | Mid="
                << std::fixed << std::setprecision(2) << snap.last_price
                << " Bid=" << snap.bid_price << " Ask=" << snap.ask_price
                << " Spread=" << snap.spread << std::endl;
    }
    ++print_count;
  });

  std::cout << "\n--- Phase 1: Bootstrapping synthetic liquidity ---\n";
  for (int i = 0; i < 12; ++i) {
    generator.step(&book, 0.0);
  }
  book.print_top_of_book();

  std::cout << "\n--- Phase 2: Integrated simulation loop ---\n";
  const std::size_t total_steps = 200;
  for (std::size_t step = 0; step < total_steps; ++step) {
    generator.step(&book, 0.45);
    simulator.process_step();

    if ((step + 1) % 40 == 0) {
      std::cout << "\n[Step " << (step + 1) << "] Order book snapshot:\n";
      book.print_top_of_book();
      std::cout << "  Fills so far: " << book.get_fills().size() << "\n";
    }
  }

  std::cout << "\n--- Phase 3: Reporting & diagnostics ---\n";
  router.print_statistics();
  simulator.print_final_report();

  std::cout << "\nIntegration demo complete. Trading simulator, strategies, "
               "market data generator, fill router, and position management "
               "are all wired together.\n";
  return 0;
}
