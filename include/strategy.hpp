#pragma once

#include "fill.hpp"
#include "order.hpp"
#include "types.hpp"
#include <deque>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class OrderBook;

// ============================================================================
// TRADING SIGNALS
// ============================================================================

enum class SignalType { BUY, SELL, HOLD, CLOSE_LONG, CLOSE_SHORT };

struct TradingSignal {
  SignalType type;
  std::string symbol;
  double confidence;   // 0.0 to 1.0
  double target_price; // Suggested limit price (0 = market order)
  int suggested_quantity;
  std::string reason; // Why this signal was generated
  TimePoint timestamp;

  TradingSignal(SignalType t, const std::string &sym, double conf = 1.0)
      : type(t), symbol(sym), confidence(conf), target_price(0.0),
        suggested_quantity(0), timestamp(Clock::now()) {}

  bool is_buy() const { return type == SignalType::BUY; }
  bool is_sell() const { return type == SignalType::SELL; }
  bool is_close() const {
    return type == SignalType::CLOSE_LONG || type == SignalType::CLOSE_SHORT;
  }
  bool is_hold() const { return type == SignalType::HOLD; }

  std::string type_to_string() const {
    switch (type) {
    case SignalType::BUY:
      return "BUY";
    case SignalType::SELL:
      return "SELL";
    case SignalType::HOLD:
      return "HOLD";
    case SignalType::CLOSE_LONG:
      return "CLOSE_LONG";
    case SignalType::CLOSE_SHORT:
      return "CLOSE_SHORT";
    default:
      return "UNKNOWN";
    }
  }
};

// ============================================================================
// MARKET DATA SNAPSHOT
// ============================================================================

struct MarketDataSnapshot {
  std::string symbol;
  double last_price;
  double bid_price;
  double ask_price;
  double bid_size;
  double ask_size;
  double spread;
  TimePoint timestamp;

  MarketDataSnapshot()
      : symbol(""), last_price(0.0), bid_price(0.0), ask_price(0.0),
        bid_size(0.0), ask_size(0.0), spread(0.0), timestamp(Clock::now()) {}
};

// ============================================================================
// STRATEGY CONFIGURATION
// ============================================================================

struct StrategyConfig {
  std::string name;
  int account_id;
  std::vector<std::string> symbols; // Symbols to trade
  double max_position_size;         // Max position per symbol
  double max_portfolio_value;       // Max total portfolio value
  bool enabled;

  // Strategy-specific parameters (stored as key-value pairs)
  std::unordered_map<std::string, double> parameters;

  StrategyConfig()
      : name(""), account_id(0), max_position_size(1000000.0),
        max_portfolio_value(10000000.0), enabled(true) {}

  double get_parameter(const std::string &key, double default_val = 0.0) const {
    auto it = parameters.find(key);
    return (it != parameters.end()) ? it->second : default_val;
  }

  void set_parameter(const std::string &key, double value) {
    parameters[key] = value;
  }
};

// ============================================================================
// STRATEGY STATISTICS
// ============================================================================

struct StrategyStats {
  int signals_generated;
  int orders_submitted;
  int orders_filled;
  int orders_rejected;
  double total_pnl;
  double win_rate;
  double sharpe_ratio;
  int trades_won;
  int trades_lost;
  TimePoint start_time;
  TimePoint last_update;

  StrategyStats()
      : signals_generated(0), orders_submitted(0), orders_filled(0),
        orders_rejected(0), total_pnl(0.0), win_rate(0.0), sharpe_ratio(0.0),
        trades_won(0), trades_lost(0), start_time(Clock::now()),
        last_update(Clock::now()) {}

  void print() const;
};

// ============================================================================
// BASE STRATEGY CLASS
// ============================================================================

class Strategy {
protected:
  StrategyConfig config_;
  StrategyStats stats_;
  bool is_initialized_;
  int next_order_id_;

  // Price history for each symbol
  std::unordered_map<std::string, std::deque<double>> price_history_;

  // Current positions (symbol -> quantity, positive=long, negative=short)
  std::unordered_map<std::string, int> positions_;

  // Pending orders that haven't been filled yet
  std::unordered_map<int, Order> pending_orders_;

public:
  Strategy(const StrategyConfig &config);
  virtual ~Strategy() = default;

  // ========================================================================
  // LIFECYCLE METHODS
  // ========================================================================

  virtual void initialize() { is_initialized_ = true; }
  virtual void shutdown() { is_initialized_ = false; }

  bool is_initialized() const { return is_initialized_; }
  bool is_enabled() const { return config_.enabled; }

  void enable() { config_.enabled = true; }
  void disable() { config_.enabled = false; }

  // ========================================================================
  // CALLBACK METHODS (Override in derived classes)
  // ========================================================================

  // Called when market data updates
  virtual void on_market_data(const MarketDataSnapshot &snapshot) = 0;

  // Called when an order is filled
  virtual void on_fill(const Fill &fill) = 0;

  // Called when an order is rejected
  virtual void on_order_rejected(int order_id, const std::string &reason);

  // Called when an order is cancelled
  virtual void on_order_cancelled(int order_id);

  // Called periodically (e.g., every second)
  virtual void on_timer();

  // ========================================================================
  // SIGNAL GENERATION (Override in derived classes)
  // ========================================================================

  // Main method to generate trading signals
  virtual std::vector<TradingSignal> generate_signals() = 0;

  // Convert signals to orders
  virtual std::vector<Order>
  signals_to_orders(const std::vector<TradingSignal> &signals);

  // ========================================================================
  // HELPER METHODS (Available to derived classes)
  // ========================================================================

  // Position management
  int get_position(const std::string &symbol) const;
  void update_position(const std::string &symbol, int quantity);
  bool has_position(const std::string &symbol) const;
  bool is_flat(const std::string &symbol) const;

  // Price history
  void add_price(const std::string &symbol, double price,
                 size_t max_history = 1000);
  const std::deque<double> &get_price_history(const std::string &symbol) const;
  double get_last_price(const std::string &symbol) const;

  // Order management
  void track_order(const Order &order);
  void remove_order(int order_id);
  bool has_pending_orders(const std::string &symbol) const;

  // Risk checks
  bool check_risk_limits(const std::string &symbol, int quantity) const;

  // ========================================================================
  // STATISTICS & REPORTING
  // ========================================================================

  const StrategyConfig &get_config() const { return config_; }
  const StrategyStats &get_stats() const { return stats_; }

  void update_stats(const Fill &fill);
  void print_summary() const;
  void print_positions() const;

  // ========================================================================
  // ACCESSORS
  // ========================================================================

  std::string get_name() const { return config_.name; }
  int get_account_id() const { return config_.account_id; }
  const std::vector<std::string> &get_symbols() const {
    return config_.symbols;
  }

protected:
  // Utility methods for derived strategies
  int generate_order_id() { return next_order_id_++; }

  // Technical indicators (helpers for strategies)
  double calculate_sma(const std::deque<double> &prices, size_t period) const;
  double calculate_ema(const std::deque<double> &prices, size_t period) const;
  double calculate_stddev(const std::deque<double> &prices,
                          size_t period) const;
  double calculate_momentum(const std::deque<double> &prices,
                            size_t period) const;
};
