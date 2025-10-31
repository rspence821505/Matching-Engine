// tests/test_helpers.hpp
#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include "order_book.hpp"
#include <gtest/gtest.h>
#include <memory>

// Helper class for common test setup
class OrderBookTest : public ::testing::Test {
protected:
  std::unique_ptr<OrderBook> book;

  void SetUp() override { book = std::make_unique<OrderBook>(); }

  void TearDown() override { book.reset(); }

  // Helper: Add a simple limit order
  void add_limit_order(int id, Side side, double price, int qty) {
    book->add_order(Order{id, side, price, qty});
  }

  // Helper: Add a market order
  void add_market_order(int id, Side side, int qty) {
    book->add_order(Order{id, side, OrderType::MARKET, qty});
  }

  // Helper: Get pending stop count
  int pending_stop_count_int() const {
    return static_cast<int>(book->pending_stop_count());
  }

  // Helper: Get fill count
  int fill_count_int() const {
    return static_cast<int>(book->get_fills().size());
  }

  // Helper: Verify fill exists
  bool has_fill(int buy_id, int sell_id, double price, int qty) {
    const auto &fills = book->get_fills();
    for (const auto &fill : fills) {
      if (fill.buy_order_id == buy_id && fill.sell_order_id == sell_id &&
          std::abs(fill.price - price) < 0.0001 && fill.quantity == qty) {
        return true;
      }
    }
    return false;
  }

  // Helper: Count total fills
  size_t fill_count() const { return book->get_fills().size(); }

  // Helper: Get best bid price
  std::optional<double> best_bid_price() const {
    auto bid = book->get_best_bid();
    return bid ? std::optional<double>(bid->price) : std::nullopt;
  }

  // Helper: Get best ask price
  std::optional<double> best_ask_price() const {
    auto ask = book->get_best_ask();
    return ask ? std::optional<double>(ask->price) : std::nullopt;
  }

  // Helper: Verify book is empty
  void assert_empty_book() {
    EXPECT_EQ(book->active_bids_count(), 0u);
    EXPECT_EQ(book->active_asks_count(), 0u);
  }

  // Helper: Verify book has orders
  void assert_book_has_orders(size_t bid_count, size_t ask_count) {
    EXPECT_EQ(book->active_bids_count(), bid_count);
    EXPECT_EQ(book->active_asks_count(), ask_count);
  }
};

// Floating point comparison helper
#define EXPECT_DOUBLE_EQ_APPROX(val1, val2) EXPECT_NEAR(val1, val2, 0.0001)

// Custom matcher for order state
#define EXPECT_ORDER_STATE(order_id, expected_state)                           \
  do {                                                                         \
    auto order = book->get_order(order_id);                                    \
    ASSERT_TRUE(order.has_value());                                            \
    EXPECT_EQ(order->state, expected_state);                                   \
  } while (0)

#endif // TEST_HELPERS_HPP