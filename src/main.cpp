#include "market_data_generator.hpp"
#include "order_book.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>

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
} // namespace

int main() {
  std::cout << "╔════════════════════════════════════════════════════════╗\n"
            << "║   Market Data Generator & Fill Router Demonstration    ║\n"
            << "╚════════════════════════════════════════════════════════╝\n"
            << std::endl;

  OrderBook book("SIMGEN");
  FillRouter &router = book.get_fill_router();

  router.set_self_trade_prevention(true);
  router.set_fee_schedule(/*maker*/ 0.0001, /*taker*/ 0.0003);

  router.register_fill_callback([](const EnhancedFill &fill) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  • Fill #" << fill.fill_id << " "
              << fill.base_fill.quantity << " @ $" << fill.base_fill.price
              << " | BuyAcct=" << fill.buy_account_id
              << " SellAcct=" << fill.sell_account_id
              << " Liquidity=" << liquidity_flag_to_string(fill.liquidity_flag)
              << "\n"
              << "    Fees (buyer=" << fill.buyer_fee
              << ", seller=" << fill.seller_fee << ")\n";
  });

  router.register_self_trade_callback(
      [](int account_id, const Order &order1, const Order &order2) {
        std::cout << "  ⚠ Self-trade prevented for account " << account_id
                  << " between orders " << order1.id << " and " << order2.id
                  << "\n";
      });

  MarketDataGenerator::Config cfg;
  cfg.symbol = "SIMGEN";
  cfg.start_price = 100.0;
  cfg.depth_levels = 3;
  cfg.volatility = 0.65;
  cfg.spread = 0.04;

  MarketDataGenerator generator(cfg);
  generator.register_callback([](const MarketDataSnapshot &snapshot) {
    std::cout << "  ≈ Snapshot " << snapshot.symbol
              << " | Mid=" << std::fixed << std::setprecision(2)
              << snapshot.last_price << " Bid=" << snapshot.bid_price
              << " Ask=" << snapshot.ask_price
              << " Spread=" << snapshot.spread << std::endl;
  });

  std::cout << "\n--- Phase 1: Seeding order book with synthetic liquidity ---\n";
  for (int i = 0; i < 6; ++i) {
    generator.step(&book, 0.35);
  }

  book.print_top_of_book();

  std::cout << "\n--- Phase 2: Self-trade prevention (enabled) ---\n";
  router.set_self_trade_prevention(true);
  generator.inject_self_trade(
      book, cfg.maker_buy_account,
      std::max(generator.current_mid(), cfg.tick_size), 75);

  std::cout << "\nRouter statistics after prevention:\n";
  router.print_statistics();

  std::cout << "\n--- Phase 3: Self-trade allowed (prevention disabled) ---\n";
  router.set_self_trade_prevention(false);
  generator.inject_self_trade(
      book, cfg.maker_buy_account + 1,
      std::max(generator.current_mid(), cfg.tick_size), 60);
  generator.step(&book, 0.5);

  std::cout << "\nFinal router statistics:\n";
  router.print_statistics();

  const auto &enhanced_fills = router.get_all_fills();
  std::cout << "\nCaptured Enhanced Fills (" << enhanced_fills.size()
            << " total):\n";
  for (const auto &fill : enhanced_fills) {
    std::cout << "  #" << fill.fill_id << "  symbol=" << fill.symbol
              << "  qty=" << fill.base_fill.quantity << "  price=$"
              << std::fixed << std::setprecision(2) << fill.base_fill.price
              << "  aggressor=" << (fill.is_aggressive_buy ? "BUY" : "SELL")
              << "  liquidity=" << liquidity_flag_to_string(fill.liquidity_flag)
              << "\n";
  }

  std::cout << "\nAccount " << cfg.maker_buy_account
            << " participation: "
            << router.get_fills_for_account(cfg.maker_buy_account).size()
            << " fills\n";
  std::cout << "Symbol " << cfg.symbol << " fills: "
            << router.get_fills_for_symbol(cfg.symbol).size() << " fills\n";

  if (!enhanced_fills.empty()) {
    uint64_t first_id = enhanced_fills.front().fill_id;
    if (EnhancedFill *first = router.get_fill_by_id(first_id)) {
      std::cout << "\nLookup by ID (" << first_id << "): "
                << first->base_fill.quantity << " @ $"
                << std::fixed << std::setprecision(2)
                << first->base_fill.price << " (buy acct "
                << first->buy_account_id << ", sell acct "
                << first->sell_account_id << ")\n";
    }
  }

  std::cout << "\nDemo complete. The market data generator and fill router are "
               "ready for testing workflows.\n";
  return 0;
}

