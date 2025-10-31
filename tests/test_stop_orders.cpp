// tests/test_stop_orders.cpp
#include "test_helpers.hpp"

TEST_F(OrderBookTest, StopLossTrigger) {
  // Add stop-sell at 98.0
  book->add_order(Order{1, Side::SELL, 98.0, 100, true});

  EXPECT_EQ(pending_stop_count_int(), 1);
  assert_empty_book(); // Not in active book yet

  // Trade at 98.0 triggers stop
  add_limit_order(2, Side::BUY, 100.0, 50);
  add_limit_order(3, Side::SELL, 98.0, 50);

  // Stop should now be active and matched
  EXPECT_EQ(book->pending_stop_count(), 0);
}

TEST_F(OrderBookTest, StopBuyTrigger) {
  // Add stop-buy at 102.0
  book->add_order(Order{1, Side::BUY, 102.0, 100, true});

  EXPECT_EQ(pending_stop_count_int(), 1);

  // Trade at 102.0 triggers stop
  add_limit_order(2, Side::SELL, 100.0, 50);
  add_limit_order(3, Side::BUY, 102.0, 50);

  EXPECT_EQ(book->pending_stop_count(), 0);
}

TEST_F(OrderBookTest, StopLimitOrder) {
  // Stop-limit: trigger at 102.0, then place limit at 101.5
  book->add_order(Order{1, Side::BUY, 102.0, 101.5, 150});

  EXPECT_EQ(pending_stop_count_int(), 1);

  // Trigger the stop
  add_limit_order(2, Side::SELL, 100.0, 50);
  add_limit_order(3, Side::BUY, 102.0, 50);

  // Stop should convert to limit order at 101.5
  EXPECT_EQ(book->pending_stop_count(), 0);

  auto bid = book->get_best_bid();
  ASSERT_TRUE(bid.has_value());
  EXPECT_EQ(bid->id, 1);
  EXPECT_DOUBLE_EQ(bid->price, 101.5);
}

TEST_F(OrderBookTest, StopDoesNotTriggerOnWrongPrice) {
  // Stop-sell at 98.0
  book->add_order(Order{1, Side::SELL, 98.0, 100, true});

  // Trade above stop price shouldn't trigger
  add_limit_order(2, Side::BUY, 100.0, 50);
  add_limit_order(3, Side::SELL, 99.0, 50);

  EXPECT_EQ(pending_stop_count_int(), 1); // Still pending
}

TEST_F(OrderBookTest, MultipleStopsTrigger) {
  book->add_order(Order{1, Side::SELL, 98.0, 100, true});
  book->add_order(Order{2, Side::SELL, 97.0, 100, true});

  EXPECT_EQ(book->pending_stop_count(), 2);

  // Trade at 97.0 should trigger both
  add_limit_order(3, Side::BUY, 100.0, 50);
  add_limit_order(4, Side::SELL, 97.0, 50);

  EXPECT_EQ(book->pending_stop_count(), 0);
}