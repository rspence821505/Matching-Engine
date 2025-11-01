#pragma once

#include "strategy.hpp"

// ============================================================================
// SIMPLE MOMENTUM STRATEGY
// ============================================================================

class MomentumStrategy : public Strategy {
private:
  int lookback_period_;
  double entry_threshold_; // Momentum % to trigger entry
  double exit_threshold_;  // Momentum % to trigger exit
  double take_profit_pct_; // Take profit at X% gain
  double stop_loss_pct_;   // Stop loss at X% loss

  std::unordered_map<std::string, double> entry_prices_;

public:
  MomentumStrategy(const StrategyConfig &config);

  void initialize() override;
  void on_market_data(const MarketDataSnapshot &snapshot) override;
  void on_fill(const Fill &fill) override;

  std::vector<TradingSignal> generate_signals() override;

private:
  TradingSignal evaluate_symbol(const std::string &symbol);
  bool should_take_profit(const std::string &symbol, double current_price);
  bool should_stop_loss(const std::string &symbol, double current_price);
};

// ============================================================================
// MEAN REVERSION STRATEGY
// ============================================================================

class MeanReversionStrategy : public Strategy {
private:
  int lookback_period_;
  double entry_std_devs_;    // Enter when price is X std devs from mean
  double exit_std_devs_;     // Exit when price returns to X std devs
  double position_size_pct_; // % of max position to use

public:
  MeanReversionStrategy(const StrategyConfig &config);

  void initialize() override;
  void on_market_data(const MarketDataSnapshot &snapshot) override;
  void on_fill(const Fill &fill) override;

  std::vector<TradingSignal> generate_signals() override;

private:
  TradingSignal evaluate_symbol(const std::string &symbol);
  double calculate_z_score(const std::string &symbol) const;
};

// ============================================================================
// MARKET MAKER STRATEGY
// ============================================================================

class MarketMakerStrategy : public Strategy {
private:
  double spread_bps_;      // Desired spread in basis points
  double inventory_limit_; // Max net position
  double quote_size_;      // Size of each quote
  double skew_factor_;     // Adjust quotes based on inventory

  struct Quote {
    double bid_price;
    double ask_price;
    int bid_size;
    int ask_size;
    TimePoint timestamp;
  };

  std::unordered_map<std::string, Quote> active_quotes_;

public:
  MarketMakerStrategy(const StrategyConfig &config);

  void initialize() override;
  void on_market_data(const MarketDataSnapshot &snapshot) override;
  void on_fill(const Fill &fill) override;
  void on_timer() override;

  std::vector<TradingSignal> generate_signals() override;

private:
  std::pair<double, double> calculate_quotes(const std::string &symbol,
                                             double mid_price);
  double calculate_inventory_skew(const std::string &symbol) const;
  bool should_update_quotes(const std::string &symbol,
                            const MarketDataSnapshot &snapshot);
};

// ============================================================================
// PAIRS TRADING STRATEGY
// ============================================================================

class PairsTradingStrategy : public Strategy {
private:
  std::string symbol1_;
  std::string symbol2_;
  int formation_period_;   // Period to calculate spread
  double entry_threshold_; // Z-score to enter
  double exit_threshold_;  // Z-score to exit

  std::deque<double> spread_history_;
  double spread_mean_;
  double spread_std_;
  bool in_trade_;

public:
  PairsTradingStrategy(const StrategyConfig &config);

  void initialize() override;
  void on_market_data(const MarketDataSnapshot &snapshot) override;
  void on_fill(const Fill &fill) override;

  std::vector<TradingSignal> generate_signals() override;

private:
  void update_spread_statistics();
  double calculate_current_spread();
  double calculate_spread_zscore();
  TradingSignal generate_pair_signal();
};

// ============================================================================
// BREAKOUT STRATEGY
// ============================================================================

class BreakoutStrategy : public Strategy {
private:
  int channel_period_;        // Period for high/low channel
  double breakout_threshold_; // % above high or below low to trigger
  double volume_multiplier_;  // Require volume X times average

  struct Channel {
    double high;
    double low;
    double avg_volume;
  };

  std::unordered_map<std::string, Channel> channels_;

public:
  BreakoutStrategy(const StrategyConfig &config);

  void initialize() override;
  void on_market_data(const MarketDataSnapshot &snapshot) override;
  void on_fill(const Fill &fill) override;

  std::vector<TradingSignal> generate_signals() override;

private:
  void update_channel(const std::string &symbol);
  TradingSignal evaluate_breakout(const std::string &symbol);
};
