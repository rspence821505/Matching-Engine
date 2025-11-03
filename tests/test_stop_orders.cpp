// tests/test_stop_orders.cpp
#include "test_helpers.hpp"

TEST_F(OrderBookTest, StopLossTrigger) {
  // Add stop-sell at 98.0
  book->add_order(Order(1, 7101, Side::SELL, 98.0, 100, true));

  EXPECT_EQ(pending_stop_count_int(), 1);
  assert_empty_book(); // Not in active book yet

  // Provide liquidity and simulate a trade printing at the stop price
  book->add_order(Order(2, 7107, Side::BUY, 98.0, 200));
  book->check_stop_triggers(98.0);

  EXPECT_EQ(book->pending_stop_count(), 0u);
  EXPECT_GT(fill_count(), 0);
}

TEST_F(OrderBookTest, StopBuyTrigger) {
  // Add stop-buy at 102.0
  book->add_order(Order(1, 7102, Side::BUY, 102.0, 100, true));

  EXPECT_EQ(pending_stop_count_int(), 1);

  // Provide liquidity and simulate a trade printing at or above the stop price
  book->add_order(Order(2, 7108, Side::SELL, 102.0, 200));
  book->check_stop_triggers(102.0);

  EXPECT_EQ(book->pending_stop_count(), 0u);
  EXPECT_GT(fill_count(), 0);
}

TEST_F(OrderBookTest, StopLimitOrder) {
  // Stop-limit: trigger at 102.0, then place limit at 101.5
  book->add_order(Order(1, 7103, Side::BUY, 102.0, 101.5, 150));

  EXPECT_EQ(pending_stop_count_int(), 1);

  // Provide liquidity and simulate a trade that triggers the stop
  book->add_order(Order(2, 7109, Side::SELL, 102.0, 200));
  book->check_stop_triggers(102.0);

  EXPECT_EQ(book->pending_stop_count(), 0u);

  auto bid = book->get_best_bid();
  ASSERT_TRUE(bid.has_value());
  EXPECT_EQ(bid->id, 1);
  EXPECT_DOUBLE_EQ(bid->price, 101.5);
}

TEST_F(OrderBookTest, StopDoesNotTriggerOnWrongPrice) {
  // Stop-sell at 98.0
  book->add_order(Order(1, 7104, Side::SELL, 98.0, 100, true));

  // Simulate a trade above the stop price -- should not trigger
  book->check_stop_triggers(99.0);

  EXPECT_EQ(pending_stop_count_int(), 1); // Still pending
}

TEST_F(OrderBookTest, MultipleStopsTrigger) {
  book->add_order(Order(1, 7105, Side::SELL, 98.0, 100, true));
  book->add_order(Order(2, 7106, Side::SELL, 97.0, 100, true));

  EXPECT_EQ(book->pending_stop_count(), 2u);

  // Provide liquidity and simulate a trade printing at 97.0
  book->add_order(Order(3, 7110, Side::BUY, 97.0, 300));
  book->check_stop_triggers(97.0);

  EXPECT_EQ(book->pending_stop_count(), 0u);
  EXPECT_GE(fill_count(), 1);
}
