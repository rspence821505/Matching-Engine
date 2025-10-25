// tests/test_iceberg_orders.cpp
#include "test_helpers.hpp"

TEST_F(OrderBookTest, IcebergOrderBasic) {
  // 500 total, 100 display
  book->add_order(Order{1, Side::SELL, 100.0, 500, 100});

  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->quantity, 500);
  EXPECT_EQ(ask->display_qty, 100);
  EXPECT_EQ(ask->hidden_qty, 400);
}

TEST_F(OrderBookTest, IcebergPartialMatch) {
  book->add_order(Order{1, Side::SELL, 100.0, 500, 100});
  add_limit_order(2, Side::BUY, 100.0, 50); // Match 50 of display

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(2, 1, 100.0, 50));

  // Iceberg should still be in book with 50 display remaining
  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->remaining_qty, 450);
  EXPECT_EQ(ask->display_qty, 50);
  EXPECT_EQ(ask->hidden_qty, 400);
}

TEST_F(OrderBookTest, IcebergRefreshAfterDisplayExhausted) {
  book->add_order(Order{1, Side::SELL, 100.0, 500, 100});
  add_limit_order(2, Side::BUY, 100.0, 100); // Exhaust display

  EXPECT_EQ(fill_count(), 1);

  // Iceberg should refresh with next 100
  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->remaining_qty, 400);
  EXPECT_EQ(ask->display_qty, 100); // Refreshed
  EXPECT_EQ(ask->hidden_qty, 300);
}

TEST_F(OrderBookTest, IcebergLosesTimePriorityOnRefresh) {
  book->add_order(Order{1, Side::SELL, 100.0, 500, 100});
  add_limit_order(2, Side::SELL, 100.0, 50); // Regular order at same price

  add_limit_order(3, Side::BUY, 100.0, 100); // Exhaust iceberg display

  // Should match against order 1 first (time priority)
  EXPECT_TRUE(has_fill(3, 1, 100.0, 100));

  // After refresh, order 1 should be behind order 2
  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->id, 2); // Regular order now has priority
}

TEST_F(OrderBookTest, IcebergFullyFilled) {
  book->add_order(Order{1, Side::SELL, 100.0, 200, 100});
  add_limit_order(2, Side::BUY, 100.0, 200); // Fill entire iceberg

  EXPECT_EQ(fill_count(), 2);              // Two partial fills
  EXPECT_TRUE(has_fill(2, 1, 100.0, 100)); // First display
  EXPECT_TRUE(has_fill(2, 1, 100.0, 100)); // After refresh

  EXPECT_ORDER_STATE(1, OrderState::FILLED);
  assert_empty_book();
}

TEST_F(OrderBookTest, IcebergWithSmallRemainder) {
  book->add_order(Order{1, Side::SELL, 100.0, 250, 100});
  add_limit_order(2, Side::BUY, 100.0, 100); // Exhaust display

  // After refresh, should show only 50 (not full peak size)
  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->remaining_qty, 150);
  EXPECT_EQ(ask->display_qty, 100);
  EXPECT_EQ(ask->hidden_qty, 50);

  add_limit_order(3, Side::BUY, 100.0, 100); // Exhaust second display

  ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->remaining_qty, 50);
  EXPECT_EQ(ask->display_qty, 50); // Last piece (less than peak)
  EXPECT_EQ(ask->hidden_qty, 0);
}

TEST_F(OrderBookTest, MultipleIcebergOrders) {
  book->add_order(Order{1, Side::SELL, 100.0, 300, 100});
  book->add_order(Order{2, Side::SELL, 100.0, 200, 50});

  add_limit_order(3, Side::BUY, 100.0, 120);

  // Should match: 100 from order 1, then 20 from order 2
  EXPECT_EQ(fill_count(), 2);
  EXPECT_TRUE(has_fill(3, 1, 100.0, 100));
  EXPECT_TRUE(has_fill(3, 2, 100.0, 20));
}