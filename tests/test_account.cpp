// tests/test_account.cpp
#include "account.hpp"
#include <gtest/gtest.h>

class AccountTest : public ::testing::Test {
protected:
  std::unique_ptr<Account> account;
  std::unordered_map<std::string, double> prices;

  void SetUp() override {
    account = std::make_unique<Account>(1, "Test Account", 100000.0);
    prices["AAPL"] = 150.0;
    prices["MSFT"] = 300.0;
  }
};

// ============================================================================
// BASIC ACCOUNT OPERATIONS
// ============================================================================

TEST_F(AccountTest, AccountCreation) {
  EXPECT_EQ(account->account_id, 1);
  EXPECT_EQ(account->name, "Test Account");
  EXPECT_DOUBLE_EQ(account->initial_cash, 100000.0);
  EXPECT_DOUBLE_EQ(account->cash_balance, 100000.0);
  EXPECT_EQ(account->total_trades, 0);
  EXPECT_DOUBLE_EQ(account->total_fees_paid, 0.0);
}

TEST_F(AccountTest, BuyReducesCash) {
  Fill fill(1, 2, 150.0, 100);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0001);

  // Cash should be reduced by (price * quantity + fee)
  double expected_cash = 100000.0 - (150.0 * 100) - (150.0 * 100 * 0.0001);
  EXPECT_NEAR(account->cash_balance, expected_cash, 0.01);
  EXPECT_EQ(account->total_trades, 1);
}

TEST_F(AccountTest, SellIncreasesCash) {
  // First buy to have position
  Fill buy_fill(1, 2, 150.0, 100);
  account->process_fill(buy_fill, Side::BUY, "AAPL", 0.0001);

  double cash_after_buy = account->cash_balance;

  // Then sell
  Fill sell_fill(2, 1, 155.0, 100);
  account->process_fill(sell_fill, Side::SELL, "AAPL", 0.0001);

  // Cash should increase by (price * quantity - fee)
  double expected_increase = (155.0 * 100) - (155.0 * 100 * 0.0001);
  EXPECT_NEAR(account->cash_balance, cash_after_buy + expected_increase, 0.01);
}

// ============================================================================
// POSITION TRACKING
// ============================================================================

TEST_F(AccountTest, OpenLongPosition) {
  Fill fill(1, 2, 150.0, 100);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0);

  ASSERT_TRUE(account->positions.find("AAPL") != account->positions.end());
  const Position &pos = account->positions["AAPL"];

  EXPECT_EQ(pos.quantity, 100);
  EXPECT_DOUBLE_EQ(pos.average_price, 150.0);
  EXPECT_TRUE(pos.is_long());
  EXPECT_FALSE(pos.is_short());
  EXPECT_FALSE(pos.is_flat());
}

TEST_F(AccountTest, OpenShortPosition) {
  Fill fill(2, 1, 150.0, 100);
  account->process_fill(fill, Side::SELL, "AAPL", 0.0);

  const Position &pos = account->positions["AAPL"];

  EXPECT_EQ(pos.quantity, -100);
  EXPECT_DOUBLE_EQ(pos.average_price, 150.0);
  EXPECT_TRUE(pos.is_short());
  EXPECT_FALSE(pos.is_long());
}

TEST_F(AccountTest, AddToLongPosition) {
  Fill fill1(1, 2, 150.0, 100);
  account->process_fill(fill1, Side::BUY, "AAPL", 0.0);

  Fill fill2(1, 2, 152.0, 50);
  account->process_fill(fill2, Side::BUY, "AAPL", 0.0);

  const Position &pos = account->positions["AAPL"];

  EXPECT_EQ(pos.quantity, 150);
  // Average should be weighted: (100*150 + 50*152) / 150 = 150.67
  EXPECT_NEAR(pos.average_price, 150.67, 0.01);
}

TEST_F(AccountTest, PartialClose) {
  Fill buy_fill(1, 2, 150.0, 100);
  account->process_fill(buy_fill, Side::BUY, "AAPL", 0.0);

  Fill sell_fill(2, 1, 155.0, 50);
  account->process_fill(sell_fill, Side::SELL, "AAPL", 0.0);

  const Position &pos = account->positions["AAPL"];

  EXPECT_EQ(pos.quantity, 50); // Half remaining
  EXPECT_DOUBLE_EQ(pos.average_price, 150.0);

  // Realized P&L should be (155 - 150) * 50 = 250
  EXPECT_DOUBLE_EQ(pos.realized_pnl, 250.0);
}

TEST_F(AccountTest, FullClose) {
  Fill buy_fill(1, 2, 150.0, 100);
  account->process_fill(buy_fill, Side::BUY, "AAPL", 0.0);

  Fill sell_fill(2, 1, 160.0, 100);
  account->process_fill(sell_fill, Side::SELL, "AAPL", 0.0);

  const Position &pos = account->positions["AAPL"];

  EXPECT_EQ(pos.quantity, 0);
  EXPECT_TRUE(pos.is_flat());

  // Realized P&L should be (160 - 150) * 100 = 1000
  EXPECT_DOUBLE_EQ(pos.realized_pnl, 1000.0);
}

TEST_F(AccountTest, ReversePosition) {
  // Start long 100
  Fill buy_fill(1, 2, 150.0, 100);
  account->process_fill(buy_fill, Side::BUY, "AAPL", 0.0);

  // Sell 150 (close 100, go short 50)
  Fill sell_fill(2, 1, 155.0, 150);
  account->process_fill(sell_fill, Side::SELL, "AAPL", 0.0);

  const Position &pos = account->positions["AAPL"];

  EXPECT_EQ(pos.quantity, -50); // Now short 50
  EXPECT_DOUBLE_EQ(pos.average_price, 155.0);

  // Realized P&L from closing long: (155 - 150) * 100 = 500
  EXPECT_DOUBLE_EQ(pos.realized_pnl, 500.0);
}

// ============================================================================
// P&L CALCULATIONS
// ============================================================================

TEST_F(AccountTest, RealizedPnL) {
  Fill buy_fill(1, 2, 150.0, 100);
  account->process_fill(buy_fill, Side::BUY, "AAPL", 0.0);

  Fill sell_fill(2, 1, 160.0, 100);
  account->process_fill(sell_fill, Side::SELL, "AAPL", 0.0);

  EXPECT_DOUBLE_EQ(account->get_total_realized_pnl(), 1000.0);
}

TEST_F(AccountTest, UnrealizedPnL) {
  Fill fill(1, 2, 150.0, 100);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0);

  prices["AAPL"] = 155.0;
  double total_pnl = account->calculate_total_pnl(prices);

  // Unrealized P&L: (155 - 150) * 100 = 500
  EXPECT_DOUBLE_EQ(total_pnl, 500.0);
}

TEST_F(AccountTest, TotalPnL) {
  // Buy 200 shares
  Fill buy1(1, 2, 150.0, 200);
  account->process_fill(buy1, Side::BUY, "AAPL", 0.0);

  // Sell 100 shares at profit
  Fill sell1(2, 1, 160.0, 100);
  account->process_fill(sell1, Side::SELL, "AAPL", 0.0);

  // Update price for unrealized P&L
  prices["AAPL"] = 165.0;
  double total_pnl = account->calculate_total_pnl(prices);

  // Realized: (160 - 150) * 100 = 1000
  // Unrealized: (165 - 150) * 100 = 1500
  // Total: 2500
  EXPECT_DOUBLE_EQ(total_pnl, 2500.0);
}

TEST_F(AccountTest, AccountValue) {
  double initial_value = account->calculate_account_value(prices);
  EXPECT_DOUBLE_EQ(initial_value, 100000.0);

  Fill fill(1, 2, 150.0, 100);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0);

  prices["AAPL"] = 155.0;
  double new_value = account->calculate_account_value(prices);

  // Cash spent: 150 * 100 = 15000
  // Position value: 155 * 100 = 15500
  // Account value: (100000 - 15000) + 15500 = 100500
  EXPECT_NEAR(new_value, 100500.0, 1.0);
}

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

TEST_F(AccountTest, WinRate) {
  // Winning trade
  Fill buy1(1, 2, 150.0, 100);
  account->process_fill(buy1, Side::BUY, "AAPL", 0.0);
  Fill sell1(2, 1, 160.0, 100);
  account->process_fill(sell1, Side::SELL, "AAPL", 0.0);

  // Losing trade
  Fill buy2(1, 2, 300.0, 50);
  account->process_fill(buy2, Side::BUY, "MSFT", 0.0);
  Fill sell2(2, 1, 290.0, 50);
  account->process_fill(sell2, Side::SELL, "MSFT", 0.0);

  EXPECT_EQ(account->winning_trades, 1);
  EXPECT_EQ(account->losing_trades, 1);
  EXPECT_DOUBLE_EQ(account->get_win_rate(), 50.0);
}

TEST_F(AccountTest, ProfitFactor) {
  // Profit: 1000
  Fill buy1(1, 2, 150.0, 100);
  account->process_fill(buy1, Side::BUY, "AAPL", 0.0);
  Fill sell1(2, 1, 160.0, 100);
  account->process_fill(sell1, Side::SELL, "AAPL", 0.0);

  // Loss: 500
  Fill buy2(1, 2, 300.0, 50);
  account->process_fill(buy2, Side::BUY, "MSFT", 0.0);
  Fill sell2(2, 1, 290.0, 50);
  account->process_fill(sell2, Side::SELL, "MSFT", 0.0);

  EXPECT_DOUBLE_EQ(account->gross_profit, 1000.0);
  EXPECT_DOUBLE_EQ(account->gross_loss, 500.0);
  EXPECT_DOUBLE_EQ(account->get_profit_factor(), 2.0);
}

TEST_F(AccountTest, ReturnOnCapital) {
  Fill buy1(1, 2, 150.0, 100);
  account->process_fill(buy1, Side::BUY, "AAPL", 0.0001);
  Fill sell1(2, 1, 160.0, 100);
  account->process_fill(sell1, Side::SELL, "AAPL", 0.0001);

  double realized_pnl = account->get_total_realized_pnl();
  double fees = account->total_fees_paid;
  double expected_roi = ((realized_pnl - fees) / 100000.0) * 100.0;

  EXPECT_NEAR(account->get_return_on_capital(), expected_roi, 0.01);
}

// ============================================================================
// RISK METRICS
// ============================================================================

TEST_F(AccountTest, Leverage) {
  Fill fill(1, 2, 150.0, 500);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0);

  double leverage = account->get_leverage(prices);

  // Exposure: 150 * 500 = 75000
  // Account value: ~100000 - 75000 + 75000 = 100000
  // Leverage: 75000 / 100000 = 0.75
  EXPECT_NEAR(leverage, 0.75, 0.05);
}

TEST_F(AccountTest, MarginUsed) {
  Fill fill(1, 2, 150.0, 100);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0);

  double margin = account->get_margin_used(prices);

  // With 100% margin: 150 * 100 = 15000
  EXPECT_DOUBLE_EQ(margin, 15000.0);
}

// ============================================================================
// MULTIPLE POSITIONS
// ============================================================================

TEST_F(AccountTest, MultipleSymbols) {
  Fill aapl_fill(1, 2, 150.0, 100);
  account->process_fill(aapl_fill, Side::BUY, "AAPL", 0.0);

  Fill msft_fill(1, 2, 300.0, 50);
  account->process_fill(msft_fill, Side::BUY, "MSFT", 0.0);

  EXPECT_EQ(account->positions.size(), 2);
  EXPECT_EQ(account->positions["AAPL"].quantity, 100);
  EXPECT_EQ(account->positions["MSFT"].quantity, 50);
}

TEST_F(AccountTest, AggregateRealizedPnL) {
  // AAPL: +1000
  Fill aapl_buy(1, 2, 150.0, 100);
  account->process_fill(aapl_buy, Side::BUY, "AAPL", 0.0);
  Fill aapl_sell(2, 1, 160.0, 100);
  account->process_fill(aapl_sell, Side::SELL, "AAPL", 0.0);

  // MSFT: -500
  Fill msft_buy(1, 2, 300.0, 50);
  account->process_fill(msft_buy, Side::BUY, "MSFT", 0.0);
  Fill msft_sell(2, 1, 290.0, 50);
  account->process_fill(msft_sell, Side::SELL, "MSFT", 0.0);

  EXPECT_DOUBLE_EQ(account->get_total_realized_pnl(), 500.0);
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_F(AccountTest, ZeroFees) {
  Fill fill(1, 2, 150.0, 100);
  account->process_fill(fill, Side::BUY, "AAPL", 0.0);

  EXPECT_DOUBLE_EQ(account->total_fees_paid, 0.0);
}

TEST_F(AccountTest, EmptyPortfolio) {
  EXPECT_DOUBLE_EQ(account->get_total_realized_pnl(), 0.0);
  EXPECT_DOUBLE_EQ(account->calculate_total_pnl(prices), 0.0);
  EXPECT_DOUBLE_EQ(account->calculate_account_value(prices), 100000.0);
}

TEST_F(AccountTest, NoTrades) {
  EXPECT_DOUBLE_EQ(account->get_win_rate(), 0.0);
  EXPECT_DOUBLE_EQ(account->get_profit_factor(), 0.0);
  EXPECT_DOUBLE_EQ(account->get_average_win(), 0.0);
  EXPECT_DOUBLE_EQ(account->get_average_loss(), 0.0);
}