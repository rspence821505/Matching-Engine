#include "strategies.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>

// ============================================================================
// MOMENTUM STRATEGY
// ============================================================================

MomentumStrategy::MomentumStrategy(const StrategyConfig &config)
    : Strategy(config) {

  // Get parameters from config
  lookback_period_ =
      static_cast<int>(config_.get_parameter("lookback_period", 20.0));
  entry_threshold_ = config_.get_parameter("entry_threshold", 2.0); // 2%
  exit_threshold_ = config_.get_parameter("exit_threshold", -0.5);  // -0.5%
  take_profit_pct_ = config_.get_parameter("take_profit", 5.0);     // 5%
  stop_loss_pct_ = config_.get_parameter("stop_loss", 2.0);         // 2%
}

void MomentumStrategy::initialize() {
  Strategy::initialize();

  std::cout << "[" << config_.name
            << "] Initialized with parameters:" << std::endl;
  std::cout << "  Lookback Period:    " << lookback_period_ << std::endl;
  std::cout << "  Entry Threshold:    " << entry_threshold_ << "%" << std::endl;
  std::cout << "  Exit Threshold:     " << exit_threshold_ << "%" << std::endl;
  std::cout << "  Take Profit:        " << take_profit_pct_ << "%" << std::endl;
  std::cout << "  Stop Loss:          " << stop_loss_pct_ << "%" << std::endl;
}

void MomentumStrategy::on_market_data(const MarketDataSnapshot &snapshot) {
  if (!is_initialized_ || !config_.enabled)
    return;

  // Update price history
  add_price(snapshot.symbol, snapshot.last_price);
}

void MomentumStrategy::on_fill(const Fill &fill) {
  update_stats(fill);

  // Track entry price for P&L calculations
  // Note: This is simplified - should match order IDs to symbols
  for (const auto &symbol : config_.symbols) {
    if (entry_prices_.find(symbol) == entry_prices_.end()) {
      entry_prices_[symbol] = fill.price;
    }
  }

  std::cout << "[" << config_.name << "] Fill received: " << fill.quantity
            << " @ $" << fill.price << std::endl;
}

std::vector<TradingSignal> MomentumStrategy::generate_signals() {
  std::vector<TradingSignal> signals;

  if (!is_initialized_ || !config_.enabled) {
    return signals;
  }

  for (const auto &symbol : config_.symbols) {
    TradingSignal signal = evaluate_symbol(symbol);
    if (!signal.is_hold()) {
      signals.push_back(signal);
      stats_.signals_generated++;
    }
  }

  return signals;
}

TradingSignal MomentumStrategy::evaluate_symbol(const std::string &symbol) {
  const auto &prices = get_price_history(symbol);

  if (prices.size() < static_cast<size_t>(lookback_period_ + 1)) {
    return TradingSignal(SignalType::HOLD, symbol);
  }

  double current_price = prices.back();
  double momentum = calculate_momentum(prices, lookback_period_);
  int position = get_position(symbol);

  // Check for take profit or stop loss first
  if (position != 0) {
    if (should_take_profit(symbol, current_price)) {
      TradingSignal signal(position > 0 ? SignalType::CLOSE_LONG
                                        : SignalType::CLOSE_SHORT,
                           symbol, 1.0);
      signal.suggested_quantity = std::abs(position);
      signal.reason = "Take profit triggered";
      return signal;
    }

    if (should_stop_loss(symbol, current_price)) {
      TradingSignal signal(position > 0 ? SignalType::CLOSE_LONG
                                        : SignalType::CLOSE_SHORT,
                           symbol, 1.0);
      signal.suggested_quantity = std::abs(position);
      signal.reason = "Stop loss triggered";
      return signal;
    }
  }

  // Entry signals
  if (position == 0) {
    if (momentum > entry_threshold_) {
      TradingSignal signal(SignalType::BUY, symbol);
      signal.confidence = std::min(momentum / (entry_threshold_ * 2), 1.0);
      signal.suggested_quantity = 100;
      signal.reason =
          "Strong positive momentum: " + std::to_string(momentum) + "%";
      return signal;
    }

    if (momentum < -entry_threshold_) {
      TradingSignal signal(SignalType::SELL, symbol);
      signal.confidence =
          std::min(std::abs(momentum) / (entry_threshold_ * 2), 1.0);
      signal.suggested_quantity = 100;
      signal.reason =
          "Strong negative momentum: " + std::to_string(momentum) + "%";
      return signal;
    }
  }

  // Exit signals
  if (position > 0 && momentum < exit_threshold_) {
    TradingSignal signal(SignalType::CLOSE_LONG, symbol);
    signal.suggested_quantity = position;
    signal.reason = "Momentum reversal (long exit)";
    return signal;
  }

  if (position < 0 && momentum > -exit_threshold_) {
    TradingSignal signal(SignalType::CLOSE_SHORT, symbol);
    signal.suggested_quantity = std::abs(position);
    signal.reason = "Momentum reversal (short exit)";
    return signal;
  }

  return TradingSignal(SignalType::HOLD, symbol);
}

bool MomentumStrategy::should_take_profit(const std::string &symbol,
                                          double current_price) {
  auto it = entry_prices_.find(symbol);
  if (it == entry_prices_.end())
    return false;

  double entry_price = it->second;
  int position = get_position(symbol);

  if (position > 0) {
    double gain_pct = ((current_price - entry_price) / entry_price) * 100.0;
    return gain_pct >= take_profit_pct_;
  } else if (position < 0) {
    double gain_pct = ((entry_price - current_price) / entry_price) * 100.0;
    return gain_pct >= take_profit_pct_;
  }

  return false;
}

bool MomentumStrategy::should_stop_loss(const std::string &symbol,
                                        double current_price) {
  auto it = entry_prices_.find(symbol);
  if (it == entry_prices_.end())
    return false;

  double entry_price = it->second;
  int position = get_position(symbol);

  if (position > 0) {
    double loss_pct = ((entry_price - current_price) / entry_price) * 100.0;
    return loss_pct >= stop_loss_pct_;
  } else if (position < 0) {
    double loss_pct = ((current_price - entry_price) / entry_price) * 100.0;
    return loss_pct >= stop_loss_pct_;
  }

  return false;
}

// ============================================================================
// MEAN REVERSION STRATEGY
// ============================================================================

MeanReversionStrategy::MeanReversionStrategy(const StrategyConfig &config)
    : Strategy(config) {

  lookback_period_ =
      static_cast<int>(config_.get_parameter("lookback_period", 20.0));
  entry_std_devs_ = config_.get_parameter("entry_std_devs", 2.0);
  exit_std_devs_ = config_.get_parameter("exit_std_devs", 0.5);
  position_size_pct_ = config_.get_parameter("position_size_pct", 100.0);
}

void MeanReversionStrategy::initialize() {
  Strategy::initialize();

  std::cout << "[" << config_.name
            << "] Initialized with parameters:" << std::endl;
  std::cout << "  Lookback Period:    " << lookback_period_ << std::endl;
  std::cout << "  Entry Std Devs:     " << entry_std_devs_ << std::endl;
  std::cout << "  Exit Std Devs:      " << exit_std_devs_ << std::endl;
  std::cout << "  Position Size:      " << position_size_pct_ << "%"
            << std::endl;
}

void MeanReversionStrategy::on_market_data(const MarketDataSnapshot &snapshot) {
  if (!is_initialized_ || !config_.enabled)
    return;

  add_price(snapshot.symbol, snapshot.last_price);
}

void MeanReversionStrategy::on_fill(const Fill &fill) {
  update_stats(fill);

  std::cout << "[" << config_.name << "] Fill received: " << fill.quantity
            << " @ $" << fill.price << std::endl;
}

std::vector<TradingSignal> MeanReversionStrategy::generate_signals() {
  std::vector<TradingSignal> signals;

  if (!is_initialized_ || !config_.enabled) {
    return signals;
  }

  for (const auto &symbol : config_.symbols) {
    TradingSignal signal = evaluate_symbol(symbol);
    if (!signal.is_hold()) {
      signals.push_back(signal);
      stats_.signals_generated++;
    }
  }

  return signals;
}

TradingSignal
MeanReversionStrategy::evaluate_symbol(const std::string &symbol) {
  const auto &prices = get_price_history(symbol);

  if (prices.size() < static_cast<size_t>(lookback_period_)) {
    return TradingSignal(SignalType::HOLD, symbol);
  }

  double z_score = calculate_z_score(symbol);
  int position = get_position(symbol);

  // Entry signals - price has deviated significantly from mean
  if (position == 0) {
    if (z_score > entry_std_devs_) {
      // Price is high - sell (expect reversion down)
      TradingSignal signal(SignalType::SELL, symbol);
      signal.confidence = std::min(z_score / (entry_std_devs_ * 2), 1.0);
      signal.suggested_quantity =
          static_cast<int>(100 * (position_size_pct_ / 100.0));
      signal.reason = "Price above " + std::to_string(entry_std_devs_) +
                      " std devs (z=" + std::to_string(z_score) + ")";
      return signal;
    }

    if (z_score < -entry_std_devs_) {
      // Price is low - buy (expect reversion up)
      TradingSignal signal(SignalType::BUY, symbol);
      signal.confidence =
          std::min(std::abs(z_score) / (entry_std_devs_ * 2), 1.0);
      signal.suggested_quantity =
          static_cast<int>(100 * (position_size_pct_ / 100.0));
      signal.reason = "Price below " + std::to_string(entry_std_devs_) +
                      " std devs (z=" + std::to_string(z_score) + ")";
      return signal;
    }
  }

  // Exit signals - price has reverted toward mean
  if (position > 0 && z_score > -exit_std_devs_) {
    // Long position, price reverted to near mean
    TradingSignal signal(SignalType::CLOSE_LONG, symbol);
    signal.suggested_quantity = position;
    signal.reason =
        "Mean reversion complete (long exit, z=" + std::to_string(z_score) +
        ")";
    return signal;
  }

  if (position < 0 && z_score < exit_std_devs_) {
    // Short position, price reverted to near mean
    TradingSignal signal(SignalType::CLOSE_SHORT, symbol);
    signal.suggested_quantity = std::abs(position);
    signal.reason =
        "Mean reversion complete (short exit, z=" + std::to_string(z_score) +
        ")";
    return signal;
  }

  return TradingSignal(SignalType::HOLD, symbol);
}

double
MeanReversionStrategy::calculate_z_score(const std::string &symbol) const {
  const auto &prices = get_price_history(symbol);

  if (prices.size() < static_cast<size_t>(lookback_period_)) {
    return 0.0;
  }

  double mean = calculate_sma(prices, lookback_period_);
  double stddev = calculate_stddev(prices, lookback_period_);

  if (stddev == 0.0)
    return 0.0;

  double current_price = prices.back();
  return (current_price - mean) / stddev;
}

// ============================================================================
// MARKET MAKER STRATEGY
// ============================================================================

MarketMakerStrategy::MarketMakerStrategy(const StrategyConfig &config)
    : Strategy(config) {

  spread_bps_ = config_.get_parameter("spread_bps", 10.0); // 10 bps
  inventory_limit_ = config_.get_parameter("inventory_limit", 500.0);
  quote_size_ = config_.get_parameter("quote_size", 100.0);
  skew_factor_ = config_.get_parameter("skew_factor", 0.1); // 10% skew
}

void MarketMakerStrategy::initialize() {
  Strategy::initialize();

  std::cout << "[" << config_.name
            << "] Initialized with parameters:" << std::endl;
  std::cout << "  Spread (bps):       " << spread_bps_ << std::endl;
  std::cout << "  Inventory Limit:    " << inventory_limit_ << std::endl;
  std::cout << "  Quote Size:         " << quote_size_ << std::endl;
  std::cout << "  Skew Factor:        " << skew_factor_ << std::endl;
}

void MarketMakerStrategy::on_market_data(const MarketDataSnapshot &snapshot) {
  if (!is_initialized_ || !config_.enabled)
    return;

  add_price(snapshot.symbol, snapshot.last_price);
}

void MarketMakerStrategy::on_fill(const Fill &fill) {
  update_stats(fill);

  std::cout << "[" << config_.name << "] Fill received: " << fill.quantity
            << " @ $" << fill.price << std::endl;
}

void MarketMakerStrategy::on_timer() {
  // Periodically review and update quotes
  // This would be called by the trading simulator
}

std::vector<TradingSignal> MarketMakerStrategy::generate_signals() {
  std::vector<TradingSignal> signals;

  if (!is_initialized_ || !config_.enabled) {
    return signals;
  }

  for (const auto &symbol : config_.symbols) {
    int position = get_position(symbol);

    // Don't add to position if we're at inventory limit
    if (std::abs(position) >= inventory_limit_) {
      continue;
    }

    double last_price = get_last_price(symbol);
    if (last_price <= 0.0)
      continue;

    auto [bid_price, ask_price] = calculate_quotes(symbol, last_price);

    // Create buy signal (bid)
    TradingSignal buy_signal(SignalType::BUY, symbol);
    buy_signal.target_price = bid_price;
    buy_signal.suggested_quantity = static_cast<int>(quote_size_);
    buy_signal.reason = "Market making bid";
    signals.push_back(buy_signal);

    // Create sell signal (ask)
    TradingSignal sell_signal(SignalType::SELL, symbol);
    sell_signal.target_price = ask_price;
    sell_signal.suggested_quantity = static_cast<int>(quote_size_);
    sell_signal.reason = "Market making ask";
    signals.push_back(sell_signal);

    stats_.signals_generated += 2;
  }

  return signals;
}

std::pair<double, double>
MarketMakerStrategy::calculate_quotes(const std::string &symbol,
                                      double mid_price) {

  double half_spread = (spread_bps_ / 10000.0) * mid_price / 2.0;
  double skew = calculate_inventory_skew(symbol);

  // Skew quotes based on inventory
  double bid_price = mid_price - half_spread + skew;
  double ask_price = mid_price + half_spread + skew;

  return {bid_price, ask_price};
}

double
MarketMakerStrategy::calculate_inventory_skew(const std::string &symbol) const {

  int position = get_position(symbol);
  double last_price = get_last_price(symbol);

  if (last_price == 0.0)
    return 0.0;

  // Skew away from large positions
  // If long, raise both bid and ask to encourage selling
  // If short, lower both bid and ask to encourage buying
  double inventory_ratio = static_cast<double>(position) / inventory_limit_;
  return inventory_ratio * skew_factor_ * last_price;
}

bool MarketMakerStrategy::should_update_quotes(
    const std::string &symbol, const MarketDataSnapshot &snapshot) {

  auto it = active_quotes_.find(symbol);
  if (it == active_quotes_.end()) {
    return true; // No active quotes, should create them
  }

  // Update if mid price has moved significantly
  double old_mid = (it->second.bid_price + it->second.ask_price) / 2.0;
  double new_mid = (snapshot.bid_price + snapshot.ask_price) / 2.0;

  double price_change_pct = std::abs((new_mid - old_mid) / old_mid) * 100.0;

  return price_change_pct > 0.1; // Update if >0.1% price change
}