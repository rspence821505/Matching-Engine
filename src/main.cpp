#include "order.hpp"
#include "order_book.hpp"
#include "fill_router.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

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
            << "║      Enhanced Fill Router Demonstration (C++17)       ║\n"
            << "╚════════════════════════════════════════════════════════╝\n"
            << std::endl;

  OrderBook book("FILL_ROUTER_DEMO");
  FillRouter &router = book.get_fill_router();

  // Configure router behaviour
  router.set_self_trade_prevention(true);
  router.set_fee_schedule(/*maker*/ 0.0001, /*taker*/ 0.0003);

  // Register callbacks so we can watch fills and self-trade handling live
  router.register_fill_callback([](const EnhancedFill &fill) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  • Fill #" << fill.fill_id << " "
              << fill.base_fill.quantity << " @ $" << fill.base_fill.price
              << "  |  BuyAcct=" << fill.buy_account_id
              << "  SellAcct=" << fill.sell_account_id
              << "  Liquidity=" << liquidity_flag_to_string(fill.liquidity_flag)
              << "\n"
              << "    Fees  (buyer=" << fill.buyer_fee
              << ", seller=" << fill.seller_fee << ")\n";
  });

  router.register_self_trade_callback(
      [](int account_id, const Order &order1, const Order &order2) {
        std::cout << "  ⚠ Self-trade prevented for account " << account_id
                  << " between orders " << order1.id << " and " << order2.id
                  << "\n";
      });

  auto submit_limit_order = [&](int id, int account, Side side, double price,
                                int quantity) {
    Order order(id, account, side, price, quantity);
    book.add_order(order);
  };

  std::cout << "\n--- Scenario 1: Standard maker/taker match ---\n";
  submit_limit_order(1, 1001, Side::SELL, 101.50, 100); // Maker posts ask
  submit_limit_order(2, 2002, Side::BUY, 101.50, 100);  // Aggressive buyer hits

  std::cout << "\n--- Scenario 2: Self-trade prevention in action ---\n";
  submit_limit_order(3, 3003, Side::SELL, 101.25, 50);
  submit_limit_order(4, 3003, Side::BUY, 101.25, 50); // Same account → blocked
  book.cancel_order(3);
  book.cancel_order(4);

  std::cout << "\n--- Scenario 3: Self-trade allowed (prevention disabled) ---\n";
  router.set_self_trade_prevention(false);
  submit_limit_order(5, 3003, Side::SELL, 101.10, 50);
  submit_limit_order(6, 3003, Side::BUY, 101.10, 50);

  // Summaries ----------------------------------------------------------------
  std::cout << "\n";
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

  std::cout << "\nAccount 3003 participation: "
            << router.get_fills_for_account(3003).size() << " fills\n";
  std::cout << "Symbol FILL_ROUTER_DEMO fills: "
            << router.get_fills_for_symbol("FILL_ROUTER_DEMO").size()
            << " fills\n";

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

  std::cout << "\nDemo complete. The enhanced fill router is configured and "
               "functioning.\n";
  return 0;
}
