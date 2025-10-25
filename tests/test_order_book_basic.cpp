// tests/test_order_book_basic.cpp
#include "test_helpers.hpp"

TEST_F(OrderBookTest, EmptyBook) {
  EXPECT_FALSE(book->get_best_bid().has_value());
  EXPECT_FALSE(book->get_best_ask().has_value());
  EXPECT_FALSE(book->get_spread().has_value());
  assert_empty_book();
}

TEST_F(OrderBookTest, AddSingleBidOrder) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  ASSERT_TRUE(book->get_best_bid().has_value());
  EXPECT_DOUBLE_EQ(book->get_best_bid()->price, 100.0);
  EXPECT_EQ(book->get_best_bid()->quantity, 100);

  assert_book_has_orders(1, 0);
}

TEST_F(OrderBookTest, AddSingleAskOrder) {
  add_limit_order(1, Side::SELL, 101.0, 100);

  ASSERT_TRUE(book->get_best_ask().has_value());
  EXPECT_DOUBLE_EQ(book->get_best_ask()->price, 101.0);
  EXPECT_EQ(book->get_best_ask()->quantity, 100);

  assert_book_has_orders(0, 1);
}

TEST_F(OrderBookTest, SpreadCalculation) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);

  ASSERT_TRUE(book->get_spread().has_value());
  EXPECT_DOUBLE_EQ_APPROX(*book->get_spread(), 1.0);
}

TEST_F(OrderBookTest, PricePriorityBids) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::BUY, 101.0, 100); // Better price
  add_limit_order(3, Side::BUY, 99.0, 100);

  ASSERT_TRUE(book->get_best_bid().has_value());
  EXPECT_DOUBLE_EQ(book->get_best_bid()->price, 101.0);
  EXPECT_EQ(book->get_best_bid()->id, 2);
}

TEST_F(OrderBookTest, PricePriorityAsks) {
  add_limit_order(1, Side::SELL, 101.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 100); // Better price
  add_limit_order(3, Side::SELL, 102.0, 100);

  ASSERT_TRUE(book->get_best_ask().has_value());
  EXPECT_DOUBLE_EQ(book->get_best_ask()->price, 100.0);
  EXPECT_EQ(book->get_best_ask()->id, 2);
}

TEST_F(OrderBookTest, TimePriority) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::BUY, 100.0, 100); // Same price, later time

  ASSERT_TRUE(book->get_best_bid().has_value());
  EXPECT_EQ(book->get_best_bid()->id, 1); // First order has priority
}

TEST_F(OrderBookTest, CancelOrder) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  assert_book_has_orders(1, 0);

  bool cancelled = book->cancel_order(1);
  EXPECT_TRUE(cancelled);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->state, OrderState::CANCELLED);
}

TEST_F(OrderBookTest, CancelNonexistentOrder) {
  bool cancelled = book->cancel_order(999);
  EXPECT_FALSE(cancelled);
}

TEST_F(OrderBookTest, AmendOrderPrice) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  bool amended = book->amend_order(1, 101.0, std::nullopt);
  EXPECT_TRUE(amended);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_DOUBLE_EQ(order->price, 101.0);
}

TEST_F(OrderBookTest, AmendOrderQuantity) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  bool amended = book->amend_order(1, std::nullopt, 200);
  EXPECT_TRUE(amended);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->remaining_qty, 200);
}