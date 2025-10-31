#include "position_manager.hpp"
#include <iostream>

int main() {
  std::cout << "╔════════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║         ACCOUNT MANAGEMENT SYSTEM DEMO                     ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝"
            << std::endl;

  // Create position manager with 1 basis point fees (0.01%)
  PositionManager pm(0.0001);

  // ========================================================================
  // PHASE 1: Create Accounts
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 1: Creating Trading Accounts" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  pm.create_account(1, "Momentum Trader", 100000.0);
  pm.create_account(2, "Market Maker", 250000.0);
  pm.create_account(3, "Arbitrage Fund", 500000.0);

  // ========================================================================
  // PHASE 2: Set Risk Limits
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 2: Setting Risk Limits" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  pm.set_risk_limits(1, 50000, 10000, 3.0);
  pm.enable_risk_limits(1);

  // ========================================================================
  // PHASE 3: Simulate Trading Activity
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 3: Simulating Trading Activity" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Set initial prices
  pm.update_price("AAPL", 150.0);
  pm.update_price("MSFT", 300.0);
  pm.update_price("GOOGL", 140.0);

  // Trade 1: Account 1 buys AAPL from Account 2
  std::cout << "\n--- Trade 1: Momentum Trader buys AAPL from Market Maker ---"
            << std::endl;
  Fill fill1(1, 2, 150.0, 100);
  pm.process_fill(fill1, 1, 2, "AAPL");

  // Trade 2: Account 1 buys more AAPL from Account 3
  std::cout << "\n--- Trade 2: Momentum Trader adds to AAPL position ---"
            << std::endl;
  Fill fill2(1, 3, 152.0, 50);
  pm.process_fill(fill2, 1, 3, "AAPL");

  // Trade 3: Account 2 buys MSFT from Account 3
  std::cout << "\n--- Trade 3: Market Maker buys MSFT ---" << std::endl;
  Fill fill3(2, 3, 300.0, 75);
  pm.process_fill(fill3, 2, 3, "MSFT");

  // Trade 4: Account 1 sells half AAPL position to Account 3 (taking profit)
  std::cout << "\n--- Trade 4: Momentum Trader takes profit on AAPL ---"
            << std::endl;
  pm.update_price("AAPL", 155.0); // Price moved up
  Fill fill4(3, 1, 155.0, 75);
  pm.process_fill(fill4, 3, 1, "AAPL");

  // Trade 5: Account 2 buys GOOGL
  std::cout << "\n--- Trade 5: Market Maker buys GOOGL ---" << std::endl;
  Fill fill5(2, 3, 140.0, 100);
  pm.process_fill(fill5, 2, 3, "GOOGL");

  // Trade 6: Account 3 buys MSFT (building position)
  std::cout << "\n--- Trade 6: Arbitrage Fund buys MSFT ---" << std::endl;
  Fill fill6(3, 2, 302.0, 50);
  pm.process_fill(fill6, 3, 2, "MSFT");

  // ========================================================================
  // PHASE 4: Mark-to-Market Price Updates
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 4: Market Price Updates" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::cout << "\nUpdating market prices..." << std::endl;
  pm.update_price("AAPL", 158.0);  // +5.3% from entry
  pm.update_price("MSFT", 305.0);  // +1.7% from entry
  pm.update_price("GOOGL", 138.0); // -1.4% from entry

  std::cout << "Current prices:" << std::endl;
  std::cout << "  AAPL:  $158.00" << std::endl;
  std::cout << "  MSFT:  $305.00" << std::endl;
  std::cout << "  GOOGL: $138.00" << std::endl;

  // ========================================================================
  // PHASE 5: Account Summaries
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 5: Account Performance Reports" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  pm.print_account_summary(1);
  std::cout << "\n" << std::string(60, '-') << std::endl;
  pm.print_account_summary(2);
  std::cout << "\n" << std::string(60, '-') << std::endl;
  pm.print_account_summary(3);

  // ========================================================================
  // PHASE 6: Aggregate Analysis
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 6: Multi-Account Analysis" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  pm.print_positions_summary();
  pm.print_aggregate_statistics();

  // ========================================================================
  // PHASE 7: Risk Limit Testing
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 7: Risk Limit Validation" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::cout << "\nChecking if Account 1 can take 500 shares of AAPL @ $158:"
            << std::endl;
  bool risk_ok = pm.check_risk_limits(1, "AAPL", 500, 158.0);
  std::cout << "Risk check result: " << (risk_ok ? "APPROVED" : "REJECTED")
            << std::endl;

  // ========================================================================
  // PHASE 8: Export Results
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 8: Exporting Results" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  pm.export_account_summary(1, "account_1_summary.txt");
  pm.export_all_accounts("all_accounts_summary.txt");

  // ========================================================================
  // PHASE 9: Performance Metrics Detail
  // ========================================================================
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "PHASE 9: Detailed Performance Breakdown" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  for (int account_id : pm.get_all_account_ids()) {
    const Account &account = pm.get_account(account_id);
    std::cout << "\n--- " << account.name << " ---" << std::endl;
    account.print_performance_metrics();
  }

  // ========================================================================
  // Summary
  // ========================================================================
  std::cout
      << "\n╔════════════════════════════════════════════════════════════╗"
      << std::endl;
  std::cout << "║                   DEMO COMPLETE                            ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\nKey Features Demonstrated:" << std::endl;
  std::cout << "  ✓ Multi-account position tracking" << std::endl;
  std::cout << "  ✓ Real-time P&L calculation (realized + unrealized)"
            << std::endl;
  std::cout << "  ✓ Transaction cost modeling (fees)" << std::endl;
  std::cout << "  ✓ Position averaging and FIFO accounting" << std::endl;
  std::cout << "  ✓ Risk limit enforcement" << std::endl;
  std::cout << "  ✓ Performance metrics (win rate, profit factor, ROI)"
            << std::endl;
  std::cout << "  ✓ Aggregate portfolio analysis" << std::endl;
  std::cout << "  ✓ Export capabilities" << std::endl;

  return 0;
}