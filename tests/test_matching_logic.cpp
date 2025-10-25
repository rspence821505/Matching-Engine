// tests/test_matching_logic.cpp
#include "test_helpers.hpp"

TEST_F(OrderBookTest, SimpleMatch) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 100);

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(1, 2, 100.0, 100));

  assert_empty_book(); // Both orders fully filled
}

TEST_F(OrderBookTest, AggressiveBuyerMarket) {
  add_limit_order(1, Side::SELL, 100.0, 100);
  add_limit_order(2, Side::BUY, 101.0, 100); // Crosses spread

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(2, 1, 100.0, 100)); // Executes at passive price (100.0)
}

TEST_F(OrderBookTest, AggressiveSellerMarket) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 99.0, 100); // Crosses spread

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(1, 2, 100.0, 100)); // Executes at passive price (100.0)
}

TEST_F(OrderBookTest, PartialFill) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 50); // Only 50 shares

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(1, 2, 100.0, 50));

  // Order 1 should have 50 remaining in book
  auto bid = book->get_best_bid();
  ASSERT_TRUE(bid.has_value());
  EXPECT_EQ(bid->id, 1);
  EXPECT_EQ(bid->remaining_qty, 50);

  EXPECT_ORDER_STATE(1, OrderState::PARTIALLY_FILLED);
  EXPECT_ORDER_STATE(2, OrderState::FILLED);
}

TEST_F(OrderBookTest, MultipleFills) {
  add_limit_order(1, Side::SELL, 100.0, 50);
  add_limit_order(2, Side::SELL, 100.0, 50);
  add_limit_order(3, Side::SELL, 100.0, 50);

  add_limit_order(4, Side::BUY, 100.0, 120); // Matches multiple asks

  EXPECT_EQ(fill_count(), 3);
  EXPECT_TRUE(has_fill(4, 1, 100.0, 50));
  EXPECT_TRUE(has_fill(4, 2, 100.0, 50));
  EXPECT_TRUE(has_fill(4, 3, 100.0, 20));

  // Order 3 should have 30 remaining
  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->id, 3);
  EXPECT_EQ(ask->remaining_qty, 30);
}

TEST_F(OrderBookTest, MarketOrderBuy) {
  add_limit_order(1, Side::SELL, 100.0, 100);
  add_market_order(2, Side::BUY, 100);

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(2, 1, 100.0, 100));
}

TEST_F(OrderBookTest, MarketOrderNoLiquidity) {
  add_market_order(1, Side::BUY, 100);

  EXPECT_EQ(fill_count(), 0);
  assert_empty_book(); // Market order cancelled (default IOC)
}

TEST_F(OrderBookTest, PriceTimePriorityMatching) {
  add_limit_order(1, Side::BUY, 100.0, 50);
  add_limit_order(2, Side::BUY, 100.0, 50); // Same price, later time

  add_limit_order(3, Side::SELL, 100.0, 75); // Partial fill of both

  EXPECT_EQ(fill_count(), 2);
  EXPECT_TRUE(has_fill(1, 3, 100.0, 50)); // Order 1 fills first (time priority)
  EXPECT_TRUE(has_fill(2, 3, 100.0, 25)); // Order 2 fills remaining

  EXPECT_ORDER_STATE(1, OrderState::FILLED);
  EXPECT_ORDER_STATE(2, OrderState::PARTIALLY_FILLED);
}

TEST_F(OrderBookTest, NoCrossedBook) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);

  // No match should occur (spread exists)
  EXPECT_EQ(fill_count(), 0);
  assert_book_has_orders(1, 1);

  ASSERT_TRUE(book->get_spread().has_value());
  EXPECT_GT(*book->get_spread(), 0); // Positive spread
}

TEST_F(OrderBookTest, VWAPCalculation) {
  add_limit_order(1, Side::SELL, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);
  add_limit_order(3, Side::BUY, 102.0, 200);

  // Should match: 100 @ $100, 100 @ $101
  // VWAP = (100*100 + 100*101) / 200 = 100.5
  EXPECT_EQ(fill_count(), 2);

  const auto &fills = book->get_fills();
  double total_notional = 0;
  int total_volume = 0;
  for (const auto &fill : fills) {
    total_notional += fill.price * fill.quantity;
    total_volume += fill.quantity;
  }

  double vwap = total_notional / total_volume;
  EXPECT_DOUBLE_EQ_APPROX(vwap, 100.5);
}