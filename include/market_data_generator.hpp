#pragma once

#include "order.hpp"
#include "strategy.hpp" // for MarketDataSnapshot

#include <deque>
#include <functional>
#include <random>
#include <vector>

class OrderBook; // Forward declaration

class MarketDataGenerator {
public:
  struct Config {
    std::string symbol = "GEN";
    double start_price = 100.0;
    double drift = 0.0;
    double volatility = 0.5;
    double spread = 0.02;
    double tick_size = 0.01;
    double min_size = 50.0;
    double max_size = 200.0;
    std::size_t depth_levels = 2;
    int seed = 1337;
    int maker_buy_account = 6001;
    int maker_sell_account = 6002;
    int taker_buy_account = 7001;
    int taker_sell_account = 7002;

    Config() = default;
  };

  using SnapshotCallback = std::function<void(const MarketDataSnapshot &)>;

  MarketDataGenerator();
  explicit MarketDataGenerator(const Config &config);

  void reset(double price);
  MarketDataSnapshot next_snapshot();
  std::vector<MarketDataSnapshot> generate_series(std::size_t steps);

  void register_callback(SnapshotCallback callback);
  void clear_callbacks();

  void step(OrderBook *book = nullptr, double market_order_probability = 0.25);
  void inject_self_trade(OrderBook &book, int account_id, double price,
                         int quantity);

  double current_mid() const { return last_mid_; }
  const Config &config() const { return config_; }

private:
  Config config_;
  std::mt19937 rng_;
  std::normal_distribution<double> price_noise_;
  std::uniform_real_distribution<double> size_dist_;
  std::uniform_real_distribution<double> uniform01_;
  double last_mid_;
  uint64_t next_order_id_;
  std::vector<SnapshotCallback> callbacks_;
  std::deque<int> resting_orders_;

  int generate_quantity();
  void emit_snapshot(const MarketDataSnapshot &snapshot);
  void submit_liquidity(OrderBook &book, const MarketDataSnapshot &snapshot);
  void maybe_submit_market(OrderBook &book, double probability);
  void maybe_cancel_resting(OrderBook &book, double probability);
};
