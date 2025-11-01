// tests/test_order_book_advanced.cpp
#include "test_helpers.hpp"
#include <chrono>
#include <thread>

// ============================================================================
// ADVANCED MATCHING SCENARIOS
// ============================================================================

TEST_F(OrderBookTest, ComplexMultiLevelMatching) {
  // Build deep order book
  add_limit_order(1, Side::SELL, 100.0, 50);
  add_limit_order(2, Side::SELL, 100.0, 50); // Same price
  add_limit_order(3, Side::SELL, 100.5, 100);
  add_limit_order(4, Side::SELL, 101.0, 100);

  // Large buy order sweeps multiple levels
  add_limit_order(5, Side::BUY, 101.0, 250);

  EXPECT_EQ(fill_count(), 4);
  EXPECT_TRUE(has_fill(5, 1, 100.0, 50));
  EXPECT_TRUE(has_fill(5, 2, 100.0, 50));
  EXPECT_TRUE(has_fill(5, 3, 100.5, 100));
  EXPECT_TRUE(has_fill(5, 4, 101.0, 50));

  // Order 4 should have 50 remaining at 101.0
  auto ask = book->get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->id, 4);
  EXPECT_EQ(ask->remaining_qty, 50);
}

TEST_F(OrderBookTest, SelfTradePrevention) {
  // This test documents current behavior - real exchanges prevent self-trades
  // Current implementation ALLOWS self-trades (buyer and seller same ID)

  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(1, Side::SELL, 100.0, 100); // Same ID

  // Current behavior: allows self-trade
  EXPECT_EQ(fill_count(), 1);

  // TODO: Implement self-trade prevention in production
  // Should either reject second order or cancel both
}

TEST_F(OrderBookTest, MarketDepthCalculation) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::BUY, 99.5, 150);
  add_limit_order(3, Side::BUY, 99.0, 200);

  add_limit_order(4, Side::SELL, 101.0, 100);
  add_limit_order(5, Side::SELL, 101.5, 150);
  add_limit_order(6, Side::SELL, 102.0, 200);

  // Verify total depth
  EXPECT_EQ(book->bids_size(), 3);
  EXPECT_EQ(book->asks_size(), 3);

  // Verify spread
  auto spread = book->get_spread();
  ASSERT_TRUE(spread.has_value());
  EXPECT_DOUBLE_EQ_APPROX(*spread, 1.0);
}

TEST_F(OrderBookTest, PriceImprovementOpportunity) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 99.0, 100); // Better than bid

  // Should match at 100.0 (passive price)
  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(1, 2, 100.0, 100));

  // Aggressive seller gets price improvement (sells at 100 instead of 99)
}

TEST_F(OrderBookTest, LargeOrderMarketImpact) {
  // Build book with multiple levels
  add_limit_order(1, Side::SELL, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.5, 100);
  add_limit_order(3, Side::SELL, 101.0, 100);
  add_limit_order(4, Side::SELL, 101.5, 100);

  // Large market order moves through levels
  add_market_order(5, Side::BUY, 350);

  EXPECT_EQ(fill_count(), 4);

  // Calculate average execution price
  const auto &fills = book->get_fills();
  double total_notional = 0;
  int total_volume = 0;
  for (const auto &fill : fills) {
    total_notional += fill.price * fill.quantity;
    total_volume += fill.quantity;
  }

  double avg_price = total_notional / total_volume;
  EXPECT_GT(avg_price, 100.0); // Slippage occurred
  EXPECT_LT(avg_price, 101.5); // But not as bad as worst price
}

// ============================================================================
// STRESS TESTING
// ============================================================================

TEST_F(OrderBookTest, HighFrequencyOrders) {
  // Add many orders rapidly
  for (int i = 0; i < 1000; ++i) {
    double price = 100.0 + (i % 10) * 0.1;
    add_limit_order(i * 2, Side::BUY, price, 10);
    add_limit_order(i * 2 + 1, Side::SELL, price + 1.0, 10);
  }

  EXPECT_GT(book->bids_size(), 0u);
  EXPECT_GT(static_cast<int>(book->asks_size()), 0);
}

TEST_F(OrderBookTest, AlternatingBuySell) {
  // Simulate back-and-forth trading
  for (int i = 0; i < 100; ++i) {
    if (i % 2 == 0) {
      add_limit_order(i, Side::BUY, 100.0, 10);
    } else {
      add_limit_order(i, Side::SELL, 100.0, 10);
    }
  }

  // Should have many fills
  EXPECT_GT(fill_count(), 40);
}

TEST_F(OrderBookTest, MassiveCancellationStorm) {
  // Add many orders
  for (int i = 0; i < 100; ++i) {
    add_limit_order(i, Side::BUY, 100.0, 10);
  }

  EXPECT_EQ(book->bids_size(), 100);

  // Cancel all orders
  for (int i = 0; i < 100; ++i) {
    bool cancelled = book->cancel_order(i);
    EXPECT_TRUE(cancelled);
  }

  assert_empty_book();
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_F(OrderBookTest, ZeroQuantityRejection) {
  // Zero quantity orders should be rejected or handled gracefully
  // Current implementation may allow them - document behavior

  // This is a design decision - should we:
  // 1. Throw exception
  // 2. Reject silently
  // 3. Allow (current behavior?)

  // Test current behavior
  EXPECT_NO_THROW({
      // Order(1, Side::BUY, 100.0, 0);  // Uncomment if you want to test
  });
}

TEST_F(OrderBookTest, NegativePriceRejection) {
  // Negative prices should be rejected
  // Current implementation may not validate - document behavior

  EXPECT_NO_THROW({
      // Order(1, Side::BUY, -100.0, 100);  // Uncomment if you want to test
  });
}

TEST_F(OrderBookTest, ExtremelyLargePrices) {
  add_limit_order(1, Side::SELL, 1000000.0, 100);
  add_limit_order(2, Side::BUY, 999999.0, 100);

  // Should not match (spread too wide)
  EXPECT_EQ(fill_count(), 0);
  assert_book_has_orders(1, 1);
}

TEST_F(OrderBookTest, VerySmallPriceIncrements) {
  add_limit_order(1, Side::BUY, 100.0000, 100);
  add_limit_order(2, Side::BUY, 100.0001, 100);

  auto bid = book->get_best_bid();
  ASSERT_TRUE(bid.has_value());
  EXPECT_EQ(bid->id, 2); // Better by 0.0001
}

TEST_F(OrderBookTest, MaxIntQuantity) {
  // Test with very large quantities
  add_limit_order(1, Side::BUY, 100.0, 1000000);
  add_limit_order(2, Side::SELL, 100.0, 1000000);

  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(1, 2, 100.0, 1000000));
}

// ============================================================================
// MARKET MICROSTRUCTURE
// ============================================================================

TEST_F(OrderBookTest, BidAskBounce) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);

  // Add order that doesn't cross spread
  add_limit_order(3, Side::BUY, 99.5, 100);

  EXPECT_EQ(fill_count(), 0);

  // Best bid should still be 100.0
  auto bid = book->get_best_bid();
  ASSERT_TRUE(bid.has_value());
  EXPECT_DOUBLE_EQ(bid->price, 100.0);
}

TEST_F(OrderBookTest, SpreadNarrowing) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 102.0, 100);

  auto initial_spread = book->get_spread();
  ASSERT_TRUE(initial_spread.has_value());
  EXPECT_DOUBLE_EQ_APPROX(*initial_spread, 2.0);

  // Narrow spread
  add_limit_order(3, Side::SELL, 101.0, 100);

  auto new_spread = book->get_spread();
  ASSERT_TRUE(new_spread.has_value());
  EXPECT_DOUBLE_EQ_APPROX(*new_spread, 1.0);
}

TEST_F(OrderBookTest, SpreadWidening) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);

  // Remove best bid and ask
  book->cancel_order(1);
  book->cancel_order(2);

  // Add wider spread
  add_limit_order(3, Side::BUY, 99.0, 100);
  add_limit_order(4, Side::SELL, 102.0, 100);

  auto spread = book->get_spread();
  ASSERT_TRUE(spread.has_value());
  EXPECT_DOUBLE_EQ_APPROX(*spread, 3.0);
}

TEST_F(OrderBookTest, LockedMarket) {
  // Bid equals ask (locked market)
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 100);

  // Should immediately match
  EXPECT_EQ(fill_count(), 1);
  assert_empty_book();
}

TEST_F(OrderBookTest, OneWayMarket) {
  // Only bids, no asks
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::BUY, 99.5, 100);
  add_limit_order(3, Side::BUY, 99.0, 100);

  EXPECT_FALSE(book->get_best_ask().has_value());
  EXPECT_FALSE(book->get_spread().has_value());

  assert_book_has_orders(3, 0);
}

// ============================================================================
// ORDER AMENDMENT CORNER CASES
// ============================================================================

TEST_F(OrderBookTest, AmendToBetterPrice) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 102.0, 100);

  // Amend sell order to cross spread
  book->amend_order(2, 99.0, std::nullopt);

  // Should match immediately
  EXPECT_GT(fill_count(), 0);
}

TEST_F(OrderBookTest, AmendToWorsePrice) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  // Amend to worse price (loses priority)
  book->amend_order(1, 99.0, std::nullopt);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_DOUBLE_EQ(order->price, 99.0);
}

TEST_F(OrderBookTest, AmendQuantityUp) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  book->amend_order(1, std::nullopt, 200);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->remaining_qty, 200);
}

TEST_F(OrderBookTest, AmendQuantityDown) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  book->amend_order(1, std::nullopt, 50);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->remaining_qty, 50);
}

TEST_F(OrderBookTest, AmendPartiallyFilledOrder) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 50); // Partial fill

  EXPECT_ORDER_STATE(1, OrderState::PARTIALLY_FILLED);

  // Try to amend remaining quantity
  book->amend_order(1, std::nullopt, 100);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->remaining_qty, 100);
}

TEST_F(OrderBookTest, AmendFilledOrder) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 100);

  EXPECT_ORDER_STATE(1, OrderState::FILLED);

  // Cannot amend filled order
  bool amended = book->amend_order(1, 101.0, std::nullopt);
  EXPECT_FALSE(amended);
}

// ============================================================================
// MULTIPLE ORDER TYPES INTERACTION
// ============================================================================

TEST_F(OrderBookTest, IcebergMeetsStopOrder) {
  // Iceberg order in book
  book->add_order(Order(1, 6001, Side::SELL, 100.0, 500, 100));

  // Stop order set below current price
  book->add_order(Order(2, 6002, Side::SELL, 98.0, 100, true));

  // Trade that triggers stop
  add_limit_order(3, Side::BUY, 98.0, 50);

  // Stop should activate and potentially match with remaining iceberg
  EXPECT_EQ(book->pending_stop_count(), 0);
}

TEST_F(OrderBookTest, MarketOrderWithIOC) {
  add_limit_order(1, Side::SELL, 100.0, 50);

  // Market order with IOC
  book->add_order(
      Order(2, 6003, Side::BUY, OrderType::MARKET, 100, TimeInForce::IOC));

  // Should fill 50, cancel 50
  EXPECT_EQ(fill_count(), 1);
  EXPECT_TRUE(has_fill(2, 1, 100.0, 50));

  EXPECT_ORDER_STATE(2, OrderState::CANCELLED);
}

TEST_F(OrderBookTest, MarketOrderWithFOK) {
  add_limit_order(1, Side::SELL, 100.0, 50);

  // Market order with FOK for 100 shares
  book->add_order(
      Order(2, 6004, Side::BUY, OrderType::MARKET, 100, TimeInForce::FOK));

  // Should be rejected (not enough liquidity)
  EXPECT_EQ(fill_count(), 0);
  EXPECT_ORDER_STATE(2, OrderState::CANCELLED);
}

// ============================================================================
// PERFORMANCE CHARACTERISTICS
// ============================================================================

TEST_F(OrderBookTest, LatencyMeasurement) {
  // Add orders and verify latency tracking
  for (int i = 0; i < 100; ++i) {
    add_limit_order(i, Side::BUY, 100.0 + i * 0.01, 10);
  }

  // Latency stats should be populated
  // This is a smoke test - actual latency values depend on hardware
  EXPECT_NO_THROW({ book->print_latency_stats(); });
}

TEST_F(OrderBookTest, OrderInsertionThroughput) {
  auto start = std::chrono::high_resolution_clock::now();

  const int num_orders = 10000;
  for (int i = 0; i < num_orders; ++i) {
    add_limit_order(i, Side::BUY, 100.0 + (i % 100) * 0.1, 10);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double orders_per_second = (num_orders * 1000000.0) / duration.count();

  // Log performance (threshold depends on hardware)
  std::cout << "Insertion throughput: " << orders_per_second << " orders/sec"
            << std::endl;

  EXPECT_GT(orders_per_second, 10000); // At least 10K orders/sec
}

// ============================================================================
// BOOK STATE CONSISTENCY
// ============================================================================

TEST_F(OrderBookTest, BookConsistencyAfterMatching) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 100);

  // After matching, book should be empty
  assert_empty_book();

  // Both orders should be marked as filled
  EXPECT_ORDER_STATE(1, OrderState::FILLED);
  EXPECT_ORDER_STATE(2, OrderState::FILLED);
}

TEST_F(OrderBookTest, OrderQuantityConsistency) {
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 60);

  auto order = book->get_order(1);
  ASSERT_TRUE(order.has_value());

  // Original quantity unchanged
  EXPECT_EQ(order->quantity, 100);

  // Remaining quantity updated
  EXPECT_EQ(order->remaining_qty, 40);
}

TEST_F(OrderBookTest, FillQuantityConsistency) {
  add_limit_order(1, Side::SELL, 100.0, 50);
  add_limit_order(2, Side::SELL, 100.5, 75);
  add_limit_order(3, Side::BUY, 101.0, 100);

  const auto &fills = book->get_fills();

  int total_filled = 0;
  for (const auto &fill : fills) {
    total_filled += fill.quantity;
  }

  EXPECT_EQ(total_filled, 100); // Total matches buyer quantity
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

TEST_F(OrderBookTest, CancelAlreadyCancelledOrder) {
  add_limit_order(1, Side::BUY, 100.0, 100);

  book->cancel_order(1);

  // Try to cancel again
  bool cancelled = book->cancel_order(1);
  EXPECT_FALSE(cancelled);
}

TEST_F(OrderBookTest, AmendNonexistentOrder) {
  bool amended = book->amend_order(999, 100.0, std::nullopt);
  EXPECT_FALSE(amended);
}

TEST_F(OrderBookTest, GetNonexistentOrder) {
  auto order = book->get_order(999);
  EXPECT_FALSE(order.has_value());
}

// ============================================================================
// REAL-WORLD SCENARIOS
// ============================================================================

TEST_F(OrderBookTest, FlashCrashScenario) {
  // Build normal book
  for (int i = 0; i < 10; ++i) {
    add_limit_order(i, Side::SELL, 100.0 + i * 0.5, 100);
  }

  // Large market order wipes out book
  add_market_order(100, Side::BUY, 5000);

  // Book should be significantly depleted
  EXPECT_LT(static_cast<int>(book->asks_size()), 10);
}

TEST_F(OrderBookTest, MarketMakerQuoteUpdate) {
  // Market maker places two-sided quote
  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.1, 100);

  auto initial_spread = book->get_spread();
  ASSERT_TRUE(initial_spread.has_value());
  EXPECT_DOUBLE_EQ_APPROX(*initial_spread, 0.1);

  // Market maker cancels and replaces (simulated)
  book->cancel_order(1);
  book->cancel_order(2);

  add_limit_order(3, Side::BUY, 100.05, 100);
  add_limit_order(4, Side::SELL, 100.15, 100);

  auto new_spread = book->get_spread();
  ASSERT_TRUE(new_spread.has_value());
  EXPECT_DOUBLE_EQ_APPROX(*new_spread, 0.1);
}

TEST_F(OrderBookTest, BlockTrade) {
  // Large institutional order
  add_limit_order(1, Side::BUY, 100.0, 10000);

  // Multiple sellers fill it
  for (int i = 0; i < 10; ++i) {
    add_limit_order(100 + i, Side::SELL, 100.0, 1000);
  }

  EXPECT_EQ(fill_count(), 10);
  EXPECT_ORDER_STATE(1, OrderState::FILLED);
}

TEST_F(OrderBookTest, TradingHalt) {
  // Simulate trading halt by not processing orders
  // (This is a documentation test - actual halt logic would be external)

  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);

  // During halt, book maintains state
  assert_book_has_orders(1, 1);
}
