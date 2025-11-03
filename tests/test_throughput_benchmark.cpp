#include "order_book.hpp"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

TEST(ThroughputTest, Meets100kMessagesPerSecond) {
  OrderBook book;
  auto start = Clock::now();

  const int NUM_ORDERS = 100000;
  for (int i = 0; i < NUM_ORDERS; ++i) {
    book.add_order(Order(i, 1000, Side::BUY, 100.0 + (i % 100) * 0.01, 10));
  }

  auto end = Clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double seconds = std::max(elapsed.count(), 1e-9);
  double throughput = NUM_ORDERS / seconds;

  EXPECT_GT(throughput, 100000); // ≥ 100k msgs/s
  std::cout << "Achieved: " << throughput << " orders/sec" << std::endl;
}
