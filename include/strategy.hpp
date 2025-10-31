#pragma once

#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// project headers
#include "fill.hpp"
#include "order.hpp"

class Strategy {
public:
  virtual ~Strategy() = default;

  // Callbacks
  virtual void on_market_data(const Order &best_bid, const Order &best_ask) = 0;
  virtual void on_fill(const Fill &fill) = 0;
  virtual void on_order_rejected(int order_id, const std::string &reason) = 0;

  // Strategy generates orders
  virtual std::vector<Order> generate_orders() = 0;

  std::string name;
  int account_id;
};

// Example implementation
class SimpleMomentumStrategy : public Strategy {
private:
  std::deque<double> price_history_;
  int lookback_period_;

public:
  void on_market_data(const Order &best_bid, const Order &best_ask) override;
  std::vector<Order> generate_orders() override;
};