#include "account.hpp"
#include "performance_metrics.hpp"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>

// Test utilities
#define TEST(name) void test_##name()
#define ASSERT_EQ(a, b) assert_equal(a, b, #a, #b, __LINE__)
#define ASSERT_NEAR(a, b, tolerance)                                           \
  assert_near(a, b, tolerance, #a, #b, __LINE__)
#define RUN_TEST(name)                                                         \
  std::cout << "Running " #name "... ";                                        \
  test_##name();                                                               \
  std::cout << "✓ PASSED" << std::endl

void assert_equal(double a, double b, const char *a_str, const char *b_str,
                  int line) {
  if (std::abs(a - b) > 1e-6) {
    std::cerr << "FAILED at line " << line << ": " << a_str << " (" << a
              << ") != " << b_str << " (" << b << ")" << std::endl;
    exit(1);
  }
}

void assert_near(double a, double b, double tolerance, const char *a_str,
                 const char *b_str, int line) {
  if (std::abs(a - b) > tolerance) {
    std::cerr << "FAILED at line " << line << ": " << a_str << " (" << a
              << ") not near " << b_str << " (" << b << ") within " << tolerance
              << std::endl;
    exit(1);
  }
}

// ============================================================================
// TESTS
// ============================================================================

TEST(empty_metrics) {
  PerformanceMetrics metrics;
  std::vector<Account> accounts;

  metrics.calculate(accounts);

  ASSERT_EQ(metrics.total_trades, 0);
  ASSERT_EQ(metrics.win_rate, 0.0);
  ASSERT_EQ(metrics.sharpe_ratio, 0.0);
  ASSERT_EQ(metrics.max_drawdown, 0.0);
}

TEST(single_account_aggregation) {
  PerformanceMetrics metrics;

  Account account(1, "Test Strategy", 100000.0);
  account.total_trades = 50;
  account.winning_trades = 30;
  account.losing_trades = 20;
  account.total_fees_paid = 250.0;

  std::vector<Account> accounts = {account};
  metrics.calculate(accounts);

  ASSERT_EQ(metrics.total_trades, 50);
  ASSERT_EQ(metrics.win_rate, 60.0); // 30/50 * 100
  ASSERT_EQ(metrics.total_fees_paid, 250.0);
}

TEST(multi_account_aggregation) {
  PerformanceMetrics metrics;

  Account account1(1, "Strategy A", 100000.0);
  account1.total_trades = 30;
  account1.winning_trades = 20;
  account1.losing_trades = 10;
  account1.total_fees_paid = 150.0;

  Account account2(2, "Strategy B", 200000.0);
  account2.total_trades = 70;
  account2.winning_trades = 40;
  account2.losing_trades = 30;
  account2.total_fees_paid = 350.0;

  std::vector<Account> accounts = {account1, account2};
  metrics.calculate(accounts);

  ASSERT_EQ(metrics.total_trades, 100);
  ASSERT_EQ(metrics.win_rate, 60.0); // (20+40)/100 * 100
  ASSERT_EQ(metrics.total_fees_paid, 500.0);
}

TEST(sharpe_ratio_uptrend) {
  PerformanceMetrics metrics;

  // Create steady uptrend (should have positive Sharpe)
  for (int i = 0; i < 100; ++i) {
    double pnl = i * 100.0; // Linear increase
    metrics.add_pnl_snapshot(Clock::now(), pnl);
  }

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  // Sharpe should be positive and significant for steady uptrend
  assert(metrics.sharpe_ratio > 1.0);
  std::cout << "  (Sharpe = " << std::fixed << std::setprecision(2)
            << metrics.sharpe_ratio << ")";
}

TEST(sharpe_ratio_flat) {
  PerformanceMetrics metrics;

  // Flat P&L (no returns)
  for (int i = 0; i < 50; ++i) {
    metrics.add_pnl_snapshot(Clock::now(), 1000.0);
  }

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  // Sharpe should be close to 0 for flat performance
  ASSERT_NEAR(metrics.sharpe_ratio, 0.0, 0.1);
}

TEST(max_drawdown_no_losses) {
  PerformanceMetrics metrics;

  // Only increasing P&L (no drawdown)
  for (int i = 0; i < 50; ++i) {
    metrics.add_pnl_snapshot(Clock::now(), i * 100.0);
  }

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  ASSERT_EQ(metrics.max_drawdown, 0.0);
}

TEST(max_drawdown_known_value) {
  PerformanceMetrics metrics;

  // Known drawdown scenario
  metrics.add_pnl_snapshot(Clock::now(), 0.0);
  metrics.add_pnl_snapshot(Clock::now(), 10000.0); // Peak
  metrics.add_pnl_snapshot(Clock::now(), 8000.0);  // 20% drawdown
  metrics.add_pnl_snapshot(Clock::now(), 9000.0);  // Recovery
  metrics.add_pnl_snapshot(Clock::now(), 7500.0);  // 25% drawdown from peak

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  // Max drawdown should be 25% (10000 -> 7500)
  ASSERT_NEAR(metrics.max_drawdown, 25.0, 0.1);
}

TEST(max_drawdown_multiple_peaks) {
  PerformanceMetrics metrics;

  metrics.add_pnl_snapshot(Clock::now(), 0.0);
  metrics.add_pnl_snapshot(Clock::now(), 5000.0);  // First peak
  metrics.add_pnl_snapshot(Clock::now(), 4000.0);  // 20% DD
  metrics.add_pnl_snapshot(Clock::now(), 10000.0); // New peak
  metrics.add_pnl_snapshot(Clock::now(), 6000.0);  // 40% DD from new peak

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  // Max DD should be 40% (from second peak)
  ASSERT_NEAR(metrics.max_drawdown, 40.0, 0.1);
}

TEST(total_return_calculation) {
  PerformanceMetrics metrics;

  metrics.add_pnl_snapshot(Clock::now(), 1000.0);
  metrics.add_pnl_snapshot(Clock::now(), 1500.0);
  metrics.add_pnl_snapshot(Clock::now(), 1800.0);

  double total_return = metrics.get_total_return();
  ASSERT_EQ(total_return, 800.0); // 1800 - 1000
}

TEST(return_percentage_calculation) {
  PerformanceMetrics metrics;

  metrics.add_pnl_snapshot(Clock::now(), 10000.0);
  metrics.add_pnl_snapshot(Clock::now(), 11000.0);
  metrics.add_pnl_snapshot(Clock::now(), 12000.0);

  double return_pct = metrics.get_return_percentage();
  ASSERT_NEAR(return_pct, 20.0, 0.1); // (12000-10000)/10000 * 100
}

TEST(calmar_ratio) {
  PerformanceMetrics metrics;

  // 20% return with 10% drawdown -> Calmar = 2.0
  metrics.add_pnl_snapshot(Clock::now(), 10000.0);
  metrics.add_pnl_snapshot(Clock::now(), 11000.0); // Peak
  metrics.add_pnl_snapshot(Clock::now(), 9900.0);  // 10% DD
  metrics.add_pnl_snapshot(Clock::now(), 12000.0); // 20% total return

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  double calmar = metrics.get_calmar_ratio();
  ASSERT_NEAR(calmar, 2.0, 0.2);
}

TEST(sortino_ratio_positive) {
  PerformanceMetrics metrics;

  // Mostly positive returns with few negative (good Sortino)
  std::vector<double> pnl_values = {1000, 1100, 1150, 1200, 1180, // Mostly up
                                    1250, 1300, 1280, 1350, 1400};

  for (double pnl : pnl_values) {
    metrics.add_pnl_snapshot(Clock::now(), pnl);
  }

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  double sortino = metrics.get_sortino_ratio();
  assert(sortino > 0); // Should be positive
  std::cout << "  (Sortino = " << std::fixed << std::setprecision(2) << sortino
            << ")";
}

TEST(timeseries_management) {
  PerformanceMetrics metrics;

  // Add some data
  for (int i = 0; i < 10; ++i) {
    metrics.add_pnl_snapshot(Clock::now(), i * 100.0);
  }

  assert(metrics.pnl_timeseries.size() == 10);

  // Clear and verify
  metrics.clear_timeseries();
  assert(metrics.pnl_timeseries.size() == 0);

  std::cout << "  (Timeseries management works)";
}

TEST(csv_export) {
  PerformanceMetrics metrics;

  for (int i = 0; i < 5; ++i) {
    metrics.add_pnl_snapshot(Clock::now(), i * 1000.0);
  }

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  // Export (will create file)
  metrics.export_to_csv("/tmp/test_metrics.csv");

  // Check file was created
  std::ifstream file("/tmp/test_metrics.csv");
  assert(file.good());
  file.close();

  std::cout << "  (CSV export successful)";
}

TEST(return_statistics) {
  PerformanceMetrics metrics;

  // Known returns
  metrics.add_pnl_snapshot(Clock::now(), 1000.0);
  metrics.add_pnl_snapshot(Clock::now(), 1100.0); // +10%
  metrics.add_pnl_snapshot(Clock::now(), 1150.0); // +4.5%
  metrics.add_pnl_snapshot(Clock::now(), 1200.0); // +4.3%

  auto [mean, stddev] = metrics.get_return_statistics();

  assert(mean > 0);      // Mean should be positive
  assert(stddev > 0);    // StdDev should be positive
  assert(stddev < mean); // For steady uptrend, volatility < mean return

  std::cout << "  (Mean = " << std::fixed << std::setprecision(4)
            << (mean * 100) << "%, StdDev = " << (stddev * 100) << "%)";
}

TEST(edge_case_single_datapoint) {
  PerformanceMetrics metrics;

  metrics.add_pnl_snapshot(Clock::now(), 1000.0);

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  // With only 1 point, metrics should be 0 or undefined
  ASSERT_EQ(metrics.sharpe_ratio, 0.0);
  ASSERT_EQ(metrics.max_drawdown, 0.0);
}

TEST(edge_case_negative_pnl) {
  PerformanceMetrics metrics;

  // Handle negative P&L correctly
  metrics.add_pnl_snapshot(Clock::now(), -1000.0);
  metrics.add_pnl_snapshot(Clock::now(), -800.0);
  metrics.add_pnl_snapshot(Clock::now(), -500.0);

  std::vector<Account> accounts;
  metrics.calculate(accounts);

  double total_return = metrics.get_total_return();
  ASSERT_EQ(total_return, 500.0); // Recovery from -1000 to -500
}

TEST(edge_case_zero_trades) {
  PerformanceMetrics metrics;

  Account account(1, "Empty Strategy", 100000.0);
  account.total_trades = 0;
  account.winning_trades = 0;
  account.losing_trades = 0;

  std::vector<Account> accounts = {account};
  metrics.calculate(accounts);

  ASSERT_EQ(metrics.win_rate, 0.0);
  ASSERT_EQ(metrics.total_trades, 0);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST(realistic_trading_scenario) {
  PerformanceMetrics metrics;

  // Simulate 6 months of trading
  Account account(1, "Trend Following", 1000000.0);
  account.total_trades = 120;
  account.winning_trades = 48; // 40% win rate (typical for trend)
  account.losing_trades = 72;
  account.gross_profit = 350000.0;
  account.gross_loss = 180000.0;
  account.total_fees_paid = 5000.0;

  std::vector<Account> accounts = {account};

  // Build realistic equity curve with trend and drawdowns
  double pnl = 0.0;
  for (int i = 0; i < 120; ++i) {
    // Add trend + noise
    pnl += 1500.0; // Uptrend

    // Add occasional drawdowns
    if (i % 20 == 0) {
      pnl -= 15000.0; // Periodic drawdown
    }

    metrics.add_pnl_snapshot(Clock::now(), pnl);
  }

  metrics.calculate(accounts);

  // Verify metrics are in reasonable ranges
  assert(metrics.sharpe_ratio > 0.5);  // Should be profitable
  assert(metrics.max_drawdown < 50.0); // Reasonable drawdown
  assert(metrics.win_rate == 40.0);    // Matches our setup

  std::cout << "  (Sharpe = " << std::fixed << std::setprecision(2)
            << metrics.sharpe_ratio << ", DD = " << metrics.max_drawdown
            << "%, Win Rate = " << metrics.win_rate << "%)";
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
  std::cout << "\n╔══════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║        PERFORMANCE METRICS UNIT TESTS                    ║"
            << std::endl;
  std::cout << "╚══════════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\n=== Basic Functionality ===" << std::endl;
  RUN_TEST(empty_metrics);
  RUN_TEST(single_account_aggregation);
  RUN_TEST(multi_account_aggregation);

  std::cout << "\n=== Sharpe Ratio ===" << std::endl;
  RUN_TEST(sharpe_ratio_uptrend);
  RUN_TEST(sharpe_ratio_flat);

  std::cout << "\n=== Maximum Drawdown ===" << std::endl;
  RUN_TEST(max_drawdown_no_losses);
  RUN_TEST(max_drawdown_known_value);
  RUN_TEST(max_drawdown_multiple_peaks);

  std::cout << "\n=== Return Calculations ===" << std::endl;
  RUN_TEST(total_return_calculation);
  RUN_TEST(return_percentage_calculation);
  RUN_TEST(return_statistics);

  std::cout << "\n=== Advanced Metrics ===" << std::endl;
  RUN_TEST(calmar_ratio);
  RUN_TEST(sortino_ratio_positive);

  std::cout << "\n=== Utility Functions ===" << std::endl;
  RUN_TEST(timeseries_management);
  RUN_TEST(csv_export);

  std::cout << "\n=== Edge Cases ===" << std::endl;
  RUN_TEST(edge_case_single_datapoint);
  RUN_TEST(edge_case_negative_pnl);
  RUN_TEST(edge_case_zero_trades);

  std::cout << "\n=== Integration Tests ===" << std::endl;
  RUN_TEST(realistic_trading_scenario);

  std::cout << "\n╔══════════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║                 ALL TESTS PASSED! ✓                      ║"
            << std::endl;
  std::cout << "╚══════════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\n📊 Test Coverage:" << std::endl;
  std::cout << "  ✓ Basic aggregation across accounts" << std::endl;
  std::cout << "  ✓ Sharpe ratio calculation" << std::endl;
  std::cout << "  ✓ Maximum drawdown tracking" << std::endl;
  std::cout << "  ✓ Return calculations (absolute and percentage)" << std::endl;
  std::cout << "  ✓ Calmar ratio" << std::endl;
  std::cout << "  ✓ Sortino ratio" << std::endl;
  std::cout << "  ✓ CSV export functionality" << std::endl;
  std::cout << "  ✓ Edge cases and error handling" << std::endl;
  std::cout << "  ✓ Realistic trading scenario" << std::endl;

  std::cout << "\nYour PerformanceMetrics implementation is working correctly!"
            << std::endl;

  return 0;
}