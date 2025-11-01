#include "order.hpp"
#include "order_book.hpp"
#include "position_manager.hpp"
#include <iomanip>
#include <iostream>

// ============================================================================
// DEMONSTRATION: Account Tracking in OrderBook
// ============================================================================

class TradingDemo {
private:
  OrderBook order_book_;
  PositionManager position_manager_;
  int next_order_id_;

public:
  TradingDemo(const std::string &symbol)
      : order_book_(symbol), next_order_id_(1) {}

  void setup_accounts() {
    std::cout << "╔════════════════════════════════════════════════════════╗"
              << std::endl;
    std::cout << "║        SETTING UP TRADING ACCOUNTS                    ║"
              << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝"
              << std::endl;
    std::cout << std::endl;

    // Create three different accounts
    position_manager_.create_account(101, "Momentum Trader", 1000000.0);
    position_manager_.create_account(102, "Market Maker", 500000.0);
    position_manager_.create_account(103, "Arbitrage Fund", 2000000.0);
  }

  void demonstrate_basic_trading() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SCENARIO 1: Basic Two-Account Trade" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Account 101 wants to buy
    Order buy_order(next_order_id_++, 101, Side::BUY, 150.0, 100);
    std::cout << "\n[Account 101] Placing buy order: " << buy_order
              << std::endl;
    order_book_.add_order(buy_order);

    // Account 102 wants to sell
    Order sell_order(next_order_id_++, 102, Side::SELL, 150.0, 100);
    std::cout << "[Account 102] Placing sell order: " << sell_order
              << std::endl;
    order_book_.add_order(sell_order);

    // Process fills
    process_fills();

    // Show results
    std::cout << "\n--- Results ---" << std::endl;
    order_book_.print_account_fills();
    position_manager_.print_positions_summary();
  }

  void demonstrate_multi_account_trading() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SCENARIO 2: Multi-Account Market Activity" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Multiple accounts placing orders
    std::cout << "\n[Phase 1] Building order book..." << std::endl;

    // Market maker (102) provides liquidity on both sides
    Order mm_bid(next_order_id_++, 102, Side::BUY, 149.50, 200);
    Order mm_ask(next_order_id_++, 102, Side::SELL, 150.50, 200);
    std::cout << "  Market Maker (102) providing liquidity" << std::endl;
    order_book_.add_order(mm_bid);
    order_book_.add_order(mm_ask);

    // Arbitrage fund (103) places large order
    Order arb_buy(next_order_id_++, 103, Side::BUY, 150.00, 500);
    std::cout << "  Arbitrage Fund (103) buying 500 shares" << std::endl;
    order_book_.add_order(arb_buy);

    // Momentum trader (101) hits the ask
    Order momentum_buy(next_order_id_++, 101, Side::BUY, 151.00, 150);
    std::cout << "  Momentum Trader (101) aggressively buying" << std::endl;
    order_book_.add_order(momentum_buy);

    // Process all fills
    process_fills();

    // Show market state
    std::cout << "\n--- Market State ---" << std::endl;
    order_book_.print_top_of_book();
    order_book_.print_account_fills();

    // Show account positions
    std::cout << "\n--- Account Positions ---" << std::endl;
    position_manager_.print_all_accounts();
  }

  void demonstrate_account_specific_queries() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SCENARIO 3: Account-Specific Queries" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Query fills for specific account
    std::cout << "\nFills for Account 101:" << std::endl;
    auto fills_101 = order_book_.get_fills_for_account(101);
    std::cout << "  Total fills: " << fills_101.size() << std::endl;
    for (const auto &af : fills_101) {
      std::cout << "  - " << af.fill.quantity << " shares @ $" << std::fixed
                << std::setprecision(2) << af.fill.price;
      if (af.buy_account_id == 101) {
        std::cout << " (BOUGHT from Account " << af.sell_account_id << ")";
      } else {
        std::cout << " (SOLD to Account " << af.buy_account_id << ")";
      }
      std::cout << std::endl;
    }

    std::cout << "\nFills for Account 102:" << std::endl;
    auto fills_102 = order_book_.get_fills_for_account(102);
    std::cout << "  Total fills: " << fills_102.size() << std::endl;
    for (const auto &af : fills_102) {
      std::cout << "  - " << af.fill.quantity << " shares @ $" << std::fixed
                << std::setprecision(2) << af.fill.price;
      if (af.buy_account_id == 102) {
        std::cout << " (BOUGHT from Account " << af.sell_account_id << ")";
      } else {
        std::cout << " (SOLD to Account " << af.buy_account_id << ")";
      }
      std::cout << std::endl;
    }
  }

  void demonstrate_order_lookup() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SCENARIO 4: Order Account Lookup" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Place an order and query it
    int test_order_id = next_order_id_++;
    Order test_order(test_order_id, 103, Side::BUY, 145.0, 50);

    std::cout << "\nPlacing order ID " << test_order_id << " for Account 103"
              << std::endl;
    order_book_.add_order(test_order);

    // Look up which account owns this order
    auto account = order_book_.get_order_account(test_order_id);
    if (account) {
      std::cout << "✓ Order " << test_order_id << " belongs to Account "
                << *account << std::endl;
    } else {
      std::cout << "✗ Order not found" << std::endl;
    }

    // Try canceling from wrong account (hypothetical auth check)
    std::cout << "\nAttempting to cancel order " << test_order_id << "..."
              << std::endl;
    if (account && *account == 103) {
      std::cout << "✓ Authorization check passed (order belongs to Account 103)"
                << std::endl;
      order_book_.cancel_order(test_order_id);
      std::cout << "✓ Order cancelled successfully" << std::endl;
    }
  }

private:
  void process_fills() {
    const auto &account_fills = order_book_.get_account_fills();
    static size_t last_processed = 0;

    // Process only new fills
    for (size_t i = last_processed; i < account_fills.size(); ++i) {
      const auto &af = account_fills[i];

      // Update current price for this symbol
      position_manager_.update_price(af.symbol, af.fill.price);

      // Route fill to appropriate accounts
      position_manager_.process_fill(af.fill, af.buy_account_id,
                                     af.sell_account_id, af.symbol);
    }

    last_processed = account_fills.size();
  }
};

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

int main() {
  std::cout << "╔════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║   ORDERBOOK ACCOUNT TRACKING DEMONSTRATION            ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════╝"
            << std::endl;

  TradingDemo demo("AAPL");

  // Setup
  demo.setup_accounts();

  // Run scenarios
  demo.demonstrate_basic_trading();
  demo.demonstrate_multi_account_trading();
  demo.demonstrate_account_specific_queries();
  demo.demonstrate_order_lookup();

  // Final summary
  std::cout << "\n╔════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║                DEMO COMPLETE                           ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\n📊 Key Features Demonstrated:" << std::endl;
  std::cout << "  ✓ Orders track account ownership" << std::endl;
  std::cout << "  ✓ Fills automatically route to correct accounts" << std::endl;
  std::cout << "  ✓ Query fills by account" << std::endl;
  std::cout << "  ✓ Look up order ownership" << std::endl;
  std::cout << "  ✓ Multi-account position tracking" << std::endl;
  std::cout << "  ✓ Automatic P&L calculation per account" << std::endl;

  std::cout << "\n🎯 Integration Benefits:" << std::endl;
  std::cout << "  • No manual account tracking needed" << std::endl;
  std::cout << "  • Fills automatically routed to PositionManager" << std::endl;
  std::cout << "  • Each account gets accurate P&L" << std::endl;
  std::cout << "  • Supports authorization checks (order ownership)"
            << std::endl;
  std::cout << "  • Ready for multi-strategy trading simulator" << std::endl;

  std::cout << "\n🔧 Next Steps:" << std::endl;
  std::cout << "  1. Apply updates to your codebase (see "
               "ACCOUNT_TRACKING_CHANGES.md)"
            << std::endl;
  std::cout << "  2. Update all Order() constructor calls to include "
               "account_id"
            << std::endl;
  std::cout << "  3. Test with your existing code" << std::endl;
  std::cout << "  4. Integrate with TradingSimulator" << std::endl;

  return 0;
}
