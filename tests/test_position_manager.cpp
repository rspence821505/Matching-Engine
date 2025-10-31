// tests/test_position_manager.cpp
#include "position_manager.hpp"
#include <gtest/gtest.h>

class PositionManagerTest : public ::testing::Test {
protected:
  std::unique_ptr<PositionManager> pm;

  void SetUp() override { pm = std::make_unique<PositionManager>(0.0001); }
};

// ============================================================================
// ACCOUNT MANAGEMENT
// ============================================================================

TEST_F(PositionManagerTest, CreateAccount) {
  pm->create_account(1, "Test Account", 100000.0);

  EXPECT_TRUE(pm->has_account(1));
  EXPECT_FALSE(pm->has_account(2));

  const Account &account = pm->get_account(1);
  EXPECT_EQ(account.account_id, 1);
  EXPECT_EQ(account.name, "Test Account");
  EXPECT_DOUBLE_EQ(account.initial_cash, 100000.0);
}

TEST_F(PositionManagerTest, DuplicateAccountThrows) {
  pm->create_account(1, "Account 1", 100000.0);

  EXPECT_THROW(pm->create_account(1, "Duplicate", 50000.0), std::runtime_error);
}

TEST_F(PositionManagerTest, NonexistentAccountThrows) {
  EXPECT_THROW(pm->get_account(999), std::runtime_error);
}

TEST_F(PositionManagerTest, GetAllAccountIds) {
  pm->create_account(3, "Account 3", 100000.0);
  pm->create_account(1, "Account 1", 100000.0);
  pm->create_account(2, "Account 2", 100000.0);

  std::vector<int> ids = pm->get_all_account_ids();

  EXPECT_EQ(ids.size(), 3);
  EXPECT_EQ(ids[0], 1); // Should be sorted
  EXPECT_EQ(ids[1], 2);
  EXPECT_EQ(ids[2], 3);
}

// ============================================================================
// FILL PROCESSING
// ============================================================================

TEST_F(PositionManagerTest, ProcessFillBetweenAccounts) {
  pm->create_account(1, "Buyer", 100000.0);
  pm->create_account(2, "Seller", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  const Account &buyer = pm->get_account(1);
  const Account &seller = pm->get_account(2);

  // Buyer should have long position
  EXPECT_EQ(buyer.positions.at("AAPL").quantity, 100);

  // Seller should have short position
  EXPECT_EQ(seller.positions.at("AAPL").quantity, -100);

  // Both should have 1 trade
  EXPECT_EQ(buyer.total_trades, 1);
  EXPECT_EQ(seller.total_trades, 1);
}

TEST_F(PositionManagerTest, FillUpdatesPrice) {
  pm->create_account(1, "Buyer", 100000.0);
  pm->create_account(2, "Seller", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  EXPECT_DOUBLE_EQ(pm->get_current_price("AAPL"), 150.0);
}

TEST_F(PositionManagerTest, MultipleFillsSameSymbol) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 100000.0);

  Fill fill1(1, 2, 150.0, 100);
  pm->process_fill(fill1, 1, 2, "AAPL");

  Fill fill2(1, 2, 152.0, 50);
  pm->process_fill(fill2, 1, 2, "AAPL");

  const Account &buyer = pm->get_account(1);
  EXPECT_EQ(buyer.positions.at("AAPL").quantity, 150);
  EXPECT_NEAR(buyer.positions.at("AAPL").average_price, 150.67, 0.01);
}

// ============================================================================
// PRICE UPDATES
// ============================================================================

TEST_F(PositionManagerTest, UpdateSinglePrice) {
  pm->update_price("AAPL", 150.0);
  EXPECT_DOUBLE_EQ(pm->get_current_price("AAPL"), 150.0);

  pm->update_price("AAPL", 155.0);
  EXPECT_DOUBLE_EQ(pm->get_current_price("AAPL"), 155.0);
}

TEST_F(PositionManagerTest, UpdateMultiplePrices) {
  std::unordered_map<std::string, double> prices;
  prices["AAPL"] = 150.0;
  prices["MSFT"] = 300.0;
  prices["GOOGL"] = 140.0;

  pm->update_prices(prices);

  EXPECT_DOUBLE_EQ(pm->get_current_price("AAPL"), 150.0);
  EXPECT_DOUBLE_EQ(pm->get_current_price("MSFT"), 300.0);
  EXPECT_DOUBLE_EQ(pm->get_current_price("GOOGL"), 140.0);
}

TEST_F(PositionManagerTest, PriceUpdateAffectsUnrealizedPnL) {
  pm->create_account(1, "Trader", 100000.0);
  pm->create_account(2, "Counterparty", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  // Update price
  pm->update_price("AAPL", 155.0);

  const Account &trader = pm->get_account(1);
  double total_pnl = trader.calculate_total_pnl(pm->get_current_prices());

  // Unrealized P&L: (155 - 150) * 100 = 500
  EXPECT_DOUBLE_EQ(total_pnl, 500.0);
}

// ============================================================================
// RISK LIMITS
// ============================================================================

TEST_F(PositionManagerTest, SetRiskLimits) {
  pm->create_account(1, "Trader", 100000.0);

  EXPECT_NO_THROW(pm->set_risk_limits(1, 50000, 10000, 3.0));
}

TEST_F(PositionManagerTest, RiskLimitsDisabledByDefault) {
  pm->create_account(1, "Trader", 100000.0);

  // Without setting limits, any trade should pass
  bool ok = pm->check_risk_limits(1, "AAPL", 1000, 150.0);
  EXPECT_TRUE(ok);
}

TEST_F(PositionManagerTest, PositionSizeLimit) {
  pm->create_account(1, "Trader", 100000.0);
  pm->set_risk_limits(1, 50000, 10000, 3.0);
  pm->enable_risk_limits(1);

  // Position value: 500 * 150 = 75000 > 50000 limit
  bool ok = pm->check_risk_limits(1, "AAPL", 500, 150.0);
  EXPECT_FALSE(ok);

  // Position value: 300 * 150 = 45000 < 50000 limit
  ok = pm->check_risk_limits(1, "AAPL", 300, 150.0);
  EXPECT_TRUE(ok);
}

TEST_F(PositionManagerTest, DisableRiskLimits) {
  pm->create_account(1, "Trader", 100000.0);
  pm->set_risk_limits(1, 50000, 10000, 3.0);
  pm->enable_risk_limits(1);

  // Should fail with limits enabled
  bool ok1 = pm->check_risk_limits(1, "AAPL", 500, 150.0);
  EXPECT_FALSE(ok1);

  // Disable limits
  pm->disable_risk_limits(1);

  // Should pass with limits disabled
  bool ok2 = pm->check_risk_limits(1, "AAPL", 500, 150.0);
  EXPECT_TRUE(ok2);
}

// ============================================================================
// AGGREGATE METRICS
// ============================================================================

TEST_F(PositionManagerTest, TotalAccountValue) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 200000.0);

  EXPECT_DOUBLE_EQ(pm->get_total_account_value(), 300000.0);
}

TEST_F(PositionManagerTest, TotalPnL) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 100000.0);

  // Trader A buys from Trader B
  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  // Price moves up
  pm->update_price("AAPL", 160.0);

  double total_pnl = pm->get_total_pnl();

  // Trader A: +1000 (long)
  // Trader B: -1000 (short)
  // Total: 0 (zero-sum game)
  EXPECT_NEAR(total_pnl, 0.0, 1.0);
}

TEST_F(PositionManagerTest, TotalFeesCollected) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  double total_fees = pm->get_total_fees_paid();

  // Both accounts pay fees
  // Fee per account: 150 * 100 * 0.0001 = 1.5
  // Total: 3.0
  EXPECT_DOUBLE_EQ(total_fees, 3.0);
}

TEST_F(PositionManagerTest, TotalTrades) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 100000.0);
  pm->create_account(3, "Trader C", 100000.0);

  Fill fill1(1, 2, 150.0, 100);
  pm->process_fill(fill1, 1, 2, "AAPL");

  Fill fill2(2, 3, 152.0, 50);
  pm->process_fill(fill2, 2, 3, "AAPL");

  // Total trades across all accounts: 1 + 2 + 1 = 4
  EXPECT_EQ(pm->get_total_trades(), 4);
}

// ============================================================================
// RESET OPERATIONS
// ============================================================================

TEST_F(PositionManagerTest, ResetAccount) {
  pm->create_account(1, "Trader", 100000.0);
  pm->create_account(2, "Counterparty", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  EXPECT_EQ(pm->get_account(1).total_trades, 1);

  pm->reset_account(1);

  const Account &reset_account = pm->get_account(1);
  EXPECT_EQ(reset_account.total_trades, 0);
  EXPECT_DOUBLE_EQ(reset_account.cash_balance, 100000.0);
  EXPECT_TRUE(reset_account.positions.empty());
}

TEST_F(PositionManagerTest, ResetAll) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  EXPECT_EQ(pm->get_all_account_ids().size(), 2);

  pm->reset();

  EXPECT_TRUE(pm->get_all_account_ids().empty());
  EXPECT_FALSE(pm->has_account(1));
  EXPECT_FALSE(pm->has_account(2));
}

// ============================================================================
// COMPLEX SCENARIOS
// ============================================================================

TEST_F(PositionManagerTest, MultiAccountMultiSymbol) {
  pm->create_account(1, "Momentum", 100000.0);
  pm->create_account(2, "Mean Reversion", 100000.0);
  pm->create_account(3, "Market Maker", 200000.0);

  // Trade AAPL
  Fill aapl_fill(1, 3, 150.0, 100);
  pm->process_fill(aapl_fill, 1, 3, "AAPL");

  // Trade MSFT
  Fill msft_fill(2, 3, 300.0, 50);
  pm->process_fill(msft_fill, 2, 3, "MSFT");

  // Trade GOOGL
  Fill googl_fill(1, 2, 140.0, 75);
  pm->process_fill(googl_fill, 1, 2, "GOOGL");

  // Check positions
  EXPECT_EQ(pm->get_account(1).positions.size(), 2); // AAPL, GOOGL
  EXPECT_EQ(pm->get_account(2).positions.size(), 2); // MSFT, GOOGL
  EXPECT_EQ(pm->get_account(3).positions.size(), 2); // AAPL, MSFT
}

TEST_F(PositionManagerTest, ClosingAndReopening) {
  pm->create_account(1, "Trader", 100000.0);
  pm->create_account(2, "Counterparty", 100000.0);

  // Open position
  Fill buy_fill(1, 2, 150.0, 100);
  pm->process_fill(buy_fill, 1, 2, "AAPL");

  // Close position
  pm->update_price("AAPL", 160.0);
  Fill sell_fill(2, 1, 160.0, 100);
  pm->process_fill(sell_fill, 2, 1, "AAPL");

  const Account &trader = pm->get_account(1);
  EXPECT_TRUE(trader.positions.at("AAPL").is_flat());
  EXPECT_DOUBLE_EQ(trader.positions.at("AAPL").realized_pnl, 1000.0);

  // Reopen position at new price
  Fill rebuy_fill(1, 2, 165.0, 50);
  pm->process_fill(rebuy_fill, 1, 2, "AAPL");

  EXPECT_EQ(trader.positions.at("AAPL").quantity, 50);
  EXPECT_DOUBLE_EQ(trader.positions.at("AAPL").average_price, 165.0);
  EXPECT_DOUBLE_EQ(trader.positions.at("AAPL").realized_pnl,
                   1000.0); // Realized P&L persists
}

TEST_F(PositionManagerTest, StressTest100Accounts) {
  // Create 100 accounts
  for (int i = 1; i <= 100; ++i) {
    pm->create_account(i, "Account " + std::to_string(i), 100000.0);
  }

  EXPECT_EQ(pm->get_all_account_ids().size(), 100);

  // Execute some trades
  for (int i = 1; i < 50; ++i) {
    Fill fill(i, i + 1, 150.0, 10);
    pm->process_fill(fill, i, i + 1, "AAPL");
  }

  EXPECT_EQ(pm->get_total_trades(), 98); // 49 trades * 2 accounts each
}

// ============================================================================
// EXPORT FUNCTIONALITY
// ============================================================================

TEST_F(PositionManagerTest, ExportAccountSummary) {
  pm->create_account(1, "Trader", 100000.0);
  pm->create_account(2, "Counterparty", 100000.0);

  Fill fill(1, 2, 150.0, 100);
  pm->process_fill(fill, 1, 2, "AAPL");

  std::string filename = "test_account_export.txt";
  EXPECT_NO_THROW(pm->export_account_summary(1, filename));

  // Clean up
  std::remove(filename.c_str());
}

TEST_F(PositionManagerTest, ExportAllAccounts) {
  pm->create_account(1, "Trader A", 100000.0);
  pm->create_account(2, "Trader B", 100000.0);

  std::string filename = "test_all_accounts_export.txt";
  EXPECT_NO_THROW(pm->export_all_accounts(filename));

  // Clean up
  std::remove(filename.c_str());
}