#include "strategy.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

// ============================================================================
// STRATEGY STATISTICS
// ============================================================================

void StrategyStats::print() const {
  std::cout << "\n=== Strategy Statistics ===" << std::endl;
  std::cout << std::string(50, '-') << std::endl;
  std::cout << "Signals Generated:   " << signals_generated << std::endl;
  std::cout << "Orders Submitted:    " << orders_submitted << std::endl;
  std::cout << "Orders Filled:       " << orders_filled << std::endl;
  std::cout << "Orders Rejected:     " << orders_rejected << std::endl;
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Total P&L:          $" << total_pnl << std::endl;
  std::cout << "Trades Won:          " << trades_won << std::endl;
  std::cout << "Trades Lost:         " << trades_lost << std::endl;

  if (trades_won + trades_lost > 0) {
    double win_rate =
        (static_cast<double>(trades_won) / (trades_won + trades_lost)) * 100.0;
    std::cout << "Win Rate:            " << win_rate << "%" << std::endl;
  }

  std::cout << std::string(50, '-') << std::endl;
}

// ============================================================================
// BASE STRATEGY
// ============================================================================

Strategy::Strategy(const StrategyConfig &config)
    : config_(config), is_initialized_(false), next_order_id_(1) {}

void Strategy::on_order_rejected(int order_id, const std::string &reason) {
  stats_.orders_rejected++;
  remove_order(order_id);

  std::cout << "[" << config_.name << "] Order " << order_id
            << " rejected: " << reason << std::endl;
}

void Strategy::on_order_cancelled(int order_id) {
  remove_order(order_id);

  std::cout << "[" << config_.name << "] Order " << order_id << " cancelled"
            << std::endl;
}

void Strategy::on_timer() {
  // Default implementation: do nothing
  // Override in derived classes for periodic actions
}

std::vector<Order>
Strategy::signals_to_orders(const std::vector<TradingSignal> &signals) {

  std::vector<Order> orders;

  for (const auto &signal : signals) {
    if (signal.is_hold()) {
      continue; // No order for HOLD signals
    }

    // Determine side
    Side side;
    if (signal.is_buy()) {
      side = Side::BUY;
    } else if (signal.is_sell() || signal.type == SignalType::CLOSE_LONG) {
      side = Side::SELL;
    } else if (signal.type == SignalType::CLOSE_SHORT) {
      side = Side::BUY;
    } else {
      continue;
    }

    // Determine quantity
    int quantity = signal.suggested_quantity;
    if (quantity <= 0) {
      // Default quantity logic
      if (signal.is_close()) {
        quantity = std::abs(get_position(signal.symbol));
      } else {
        quantity = 100; // Default
      }
    }

    // Check risk limits
    if (!check_risk_limits(signal.symbol, quantity)) {
      std::cout << "[" << config_.name << "] Risk limit exceeded for "
                << signal.symbol << ", skipping signal" << std::endl;
      continue;
    }

    // Create order
    int order_id = generate_order_id();

    // If target_price is 0 or very close to 0, make it a market order
    Order order =
        (std::abs(signal.target_price) < 0.01)
            ? Order(order_id, config_.account_id, side, OrderType::MARKET,
                    quantity)
            : Order(order_id, config_.account_id, side, signal.target_price,
                    quantity);

    orders.push_back(order);
    track_order(order);

    stats_.orders_submitted++;
  }

  return orders;
}

// ============================================================================
// POSITION MANAGEMENT
// ============================================================================

int Strategy::get_position(const std::string &symbol) const {
  auto it = positions_.find(symbol);
  return (it != positions_.end()) ? it->second : 0;
}

void Strategy::update_position(const std::string &symbol, int quantity) {
  positions_[symbol] = quantity;
}

bool Strategy::has_position(const std::string &symbol) const {
  return positions_.find(symbol) != positions_.end() &&
         positions_.at(symbol) != 0;
}

bool Strategy::is_flat(const std::string &symbol) const {
  return get_position(symbol) == 0;
}

// ============================================================================
// PRICE HISTORY
// ============================================================================

void Strategy::add_price(const std::string &symbol, double price,
                         size_t max_history) {
  auto &history = price_history_[symbol];
  history.push_back(price);

  if (history.size() > max_history) {
    history.pop_front();
  }
}

const std::deque<double> &
Strategy::get_price_history(const std::string &symbol) const {

  static std::deque<double> empty;
  auto it = price_history_.find(symbol);
  return (it != price_history_.end()) ? it->second : empty;
}

double Strategy::get_last_price(const std::string &symbol) const {
  const auto &history = get_price_history(symbol);
  return history.empty() ? 0.0 : history.back();
}

// ============================================================================
// ORDER MANAGEMENT
// ============================================================================

void Strategy::track_order(const Order &order) {
  pending_orders_.insert({order.id, order});
}

void Strategy::remove_order(int order_id) { pending_orders_.erase(order_id); }

bool Strategy::has_pending_orders(const std::string & /* symbol */) const {
  // Note: We'd need to track symbol per order for this to work perfectly
  // For now, just check if we have any pending orders
  return !pending_orders_.empty();
}

// ============================================================================
// RISK MANAGEMENT
// ============================================================================

bool Strategy::check_risk_limits(const std::string &symbol,
                                 int quantity) const {
  // Check position size limit
  int current_position = get_position(symbol);
  int new_position = std::abs(current_position + quantity);

  if (new_position > config_.max_position_size) {
    return false;
  }

  return true;
}

// ============================================================================
// STATISTICS
// ============================================================================

void Strategy::update_stats(const Fill &fill) {
  stats_.orders_filled++;
  stats_.last_update = Clock::now();

  // Remove from pending orders
  remove_order(fill.buy_order_id);
  remove_order(fill.sell_order_id);
}

void Strategy::print_summary() const {
  std::cout << "\n╔════════════════════════════════════════════════════╗"
            << std::endl;
  std::cout << "║  Strategy: " << std::left << std::setw(39) << config_.name
            << "║" << std::endl;
  std::cout << "╚════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\nConfiguration:" << std::endl;
  std::cout << "  Account ID:         " << config_.account_id << std::endl;
  std::cout << "  Status:             "
            << (config_.enabled ? "ENABLED" : "DISABLED") << std::endl;
  std::cout << "  Symbols:            ";
  for (size_t i = 0; i < config_.symbols.size(); ++i) {
    std::cout << config_.symbols[i];
    if (i < config_.symbols.size() - 1)
      std::cout << ", ";
  }
  std::cout << std::endl;
  std::cout << "  Max Position Size:  " << config_.max_position_size
            << std::endl;

  stats_.print();
}

void Strategy::print_positions() const {
  std::cout << "\n=== Current Positions ===" << std::endl;

  if (positions_.empty()) {
    std::cout << "No positions." << std::endl;
    return;
  }

  for (const auto &[symbol, quantity] : positions_) {
    if (quantity == 0)
      continue;

    std::cout << symbol << ": " << quantity
              << (quantity > 0 ? " (LONG)" : " (SHORT)") << std::endl;
  }
}

// ============================================================================
// TECHNICAL INDICATORS
// ============================================================================

double Strategy::calculate_sma(const std::deque<double> &prices,
                               size_t period) const {
  if (prices.size() < period) {
    return 0.0;
  }

  double sum = 0.0;
  for (size_t i = prices.size() - period; i < prices.size(); ++i) {
    sum += prices[i];
  }

  return sum / period;
}

double Strategy::calculate_ema(const std::deque<double> &prices,
                               size_t period) const {
  if (prices.size() < period) {
    return 0.0;
  }

  // Start with SMA for first EMA value
  double ema = calculate_sma(prices, period);
  double multiplier = 2.0 / (period + 1.0);

  // Calculate EMA for remaining prices
  for (size_t i = period; i < prices.size(); ++i) {
    ema = (prices[i] - ema) * multiplier + ema;
  }

  return ema;
}

double Strategy::calculate_stddev(const std::deque<double> &prices,
                                  size_t period) const {
  if (prices.size() < period) {
    return 0.0;
  }

  double mean = calculate_sma(prices, period);

  double sum_squared_diff = 0.0;
  for (size_t i = prices.size() - period; i < prices.size(); ++i) {
    double diff = prices[i] - mean;
    sum_squared_diff += diff * diff;
  }

  return std::sqrt(sum_squared_diff / period);
}

double Strategy::calculate_momentum(const std::deque<double> &prices,
                                    size_t period) const {
  if (prices.size() < period + 1) {
    return 0.0;
  }

  size_t current_idx = prices.size() - 1;
  size_t past_idx = current_idx - period;

  double current = prices[current_idx];
  double past = prices[past_idx];

  if (past == 0.0)
    return 0.0;

  return ((current - past) / past) * 100.0; // Percentage change
}
