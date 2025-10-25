// tests/test_order.cpp
#include "order.hpp"
#include <gtest/gtest.h>

TEST(OrderTest, LimitOrderCreation) {
  Order order(1, Side::BUY, 100.0, 200);

  EXPECT_EQ(order.id, 1);
  EXPECT_EQ(order.side, Side::BUY);
  EXPECT_EQ(order.type, OrderType::LIMIT);
  EXPECT_DOUBLE_EQ(order.price, 100.0);
  EXPECT_EQ(order.quantity, 200);
  EXPECT_EQ(order.remaining_qty, 200);
  EXPECT_EQ(order.state, OrderState::PENDING);
  EXPECT_FALSE(order.is_filled());
}

TEST(OrderTest, MarketOrderCreation) {
  Order order(2, Side::SELL, OrderType::MARKET, 100);

  EXPECT_EQ(order.id, 2);
  EXPECT_EQ(order.side, Side::SELL);
  EXPECT_EQ(order.type, OrderType::MARKET);
  EXPECT_TRUE(order.is_market_order());
  EXPECT_EQ(order.quantity, 100);
}

TEST(OrderTest, IcebergOrderCreation) {
  Order order(3, Side::BUY, 100.0, 500, 100); // 500 total, 100 display

  EXPECT_TRUE(order.is_iceberg());
  EXPECT_EQ(order.quantity, 500);
  EXPECT_EQ(order.display_qty, 100);
  EXPECT_EQ(order.hidden_qty, 400);
  EXPECT_EQ(order.peak_size, 100);
}

TEST(OrderTest, IcebergRefresh) {
  Order order(4, Side::BUY, 100.0, 500, 100);

  // Exhaust display quantity
  order.display_qty = 0;
  order.hidden_qty = 400;

  EXPECT_TRUE(order.needs_refresh());

  order.refresh_display();

  EXPECT_EQ(order.display_qty, 100);
  EXPECT_EQ(order.hidden_qty, 300);
  EXPECT_FALSE(order.needs_refresh());
}

TEST(OrderTest, StopOrderCreation) {
  // Stop-market order
  Order stop_market(5, Side::SELL, 98.0, 100, true);

  EXPECT_TRUE(stop_market.is_stop_order());
  EXPECT_DOUBLE_EQ(stop_market.stop_price, 98.0);
  EXPECT_FALSE(stop_market.stop_triggered);
  EXPECT_EQ(stop_market.stop_becomes, OrderType::MARKET);

  // Stop-limit order
  Order stop_limit(6, Side::BUY, 102.0, 101.5, 150);

  EXPECT_TRUE(stop_limit.is_stop_order());
  EXPECT_DOUBLE_EQ(stop_limit.stop_price, 102.0);
  EXPECT_DOUBLE_EQ(stop_limit.price, 101.5);
  EXPECT_EQ(stop_limit.stop_becomes, OrderType::LIMIT);
}

TEST(OrderTest, OrderStateTransitions) {
  Order order(7, Side::BUY, 100.0, 100);

  EXPECT_EQ(order.state, OrderState::PENDING);
  EXPECT_FALSE(order.is_active());

  order.state = OrderState::ACTIVE;
  EXPECT_TRUE(order.is_active());

  order.remaining_qty = 50;
  EXPECT_FALSE(order.is_filled());

  order.remaining_qty = 0;
  EXPECT_TRUE(order.is_filled());
}

TEST(OrderTest, TimeInForce) {
  Order gtc(1, Side::BUY, 100.0, 100, TimeInForce::GTC);
  EXPECT_TRUE(gtc.can_rest_in_book());

  Order ioc(2, Side::BUY, 100.0, 100, TimeInForce::IOC);
  EXPECT_FALSE(ioc.can_rest_in_book());

  Order fok(3, Side::BUY, 100.0, 100, TimeInForce::FOK);
  EXPECT_FALSE(fok.can_rest_in_book());

  Order day(4, Side::BUY, 100.0, 100, TimeInForce::DAY);
  EXPECT_TRUE(day.can_rest_in_book());
}

TEST(OrderTest, BidComparator) {
  Order high_price(1, Side::BUY, 101.0, 100);
  Order low_price(2, Side::BUY, 100.0, 100);

  BidComparator comp;

  // Higher price should have priority (return false means high_price wins)
  EXPECT_FALSE(comp(high_price, low_price));
  EXPECT_TRUE(comp(low_price, high_price));
}

TEST(OrderTest, AskComparator) {
  Order low_price(1, Side::SELL, 100.0, 100);
  Order high_price(2, Side::SELL, 101.0, 100);

  AskComparator comp;

  // Lower price should have priority (return false means low_price wins)
  EXPECT_FALSE(comp(low_price, high_price));
  EXPECT_TRUE(comp(high_price, low_price));
}