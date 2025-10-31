// tests/test_tif_policies.cpp
#include "test_helpers.hpp"

TEST_F(OrderBookTest, GTCOrderRestsInBook) {
  book->add_order(Order{1, Side::BUY, 100.0, 100, TimeInForce::GTC});

  // No match, should rest in book
  assert_book_has_orders(1, 0);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->state, OrderState::ACTIVE);
}

TEST_F(OrderBookTest, IOCFullyFilled) {
  add_limit_order(1, Side::SELL, 100.0, 100);
  book->add_order(Order{2, Side::BUY, 100.0, 100, TimeInForce::IOC});

  EXPECT_EQ(fill_count(), 1);
  EXPECT_ORDER_STATE(2, OrderState::FILLED);
}

TEST_F(OrderBookTest, IOCPartiallyFilled) {
  add_limit_order(1, Side::SELL, 100.0, 50);
  book->add_order(Order{2, Side::BUY, 100.0, 100, TimeInForce::IOC});

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(2, 1, 100.0, 50));

  // Remaining 50 should be cancelled
  auto order = book->get_order(2);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->state, OrderState::CANCELLED);

  assert_empty_book(); // IOC doesn't rest
}

TEST_F(OrderBookTest, IOCNoFill) {
  book->add_order(Order{1, Side::BUY, 100.0, 100, TimeInForce::IOC});

  EXPECT_EQ(fill_count_int(), 0);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->state, OrderState::CANCELLED);
}

TEST_F(OrderBookTest, FOKFullyFilled) {
  add_limit_order(1, Side::SELL, 100.0, 100);
  book->add_order(Order{2, Side::BUY, 100.0, 100, TimeInForce::FOK});

  EXPECT_EQ(fill_count(), 1);
  EXPECT_ORDER_STATE(2, OrderState::FILLED);
}

TEST_F(OrderBookTest, FOKInsufficientLiquidity) {
  add_limit_order(1, Side::SELL, 100.0, 50); // Only 50 available
  book->add_order(
      Order{2, Side::BUY, 100.0, 100, TimeInForce::FOK}); // Wants 100

  EXPECT_EQ(fill_count_int(), 0); // All-or-nothing: no fill

  auto order = book->get_order(2);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->state, OrderState::CANCELLED);

  // Original order should still be in book
  assert_book_has_orders(0, 1);
}

TEST_F(OrderBookTest, FOKMultipleLevels) {
  add_limit_order(1, Side::SELL, 100.0, 50);
  add_limit_order(2, Side::SELL, 100.5, 50);

  // FOK for 100 should fill across both levels
  book->add_order(Order{3, Side::BUY, 101.0, 100, TimeInForce::FOK});

  EXPECT_EQ(fill_count(), 2);
  EXPECT_ORDER_STATE(3, OrderState::FILLED);
}

TEST_F(OrderBookTest, DAYOrderRestsInBook) {
  book->add_order(Order{1, Side::BUY, 100.0, 100, TimeInForce::DAY});

  assert_book_has_orders(1, 0);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->state, OrderState::ACTIVE);
}