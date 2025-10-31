#include "account.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

void Account::process_fill(const Fill &fill, Side side,
                           const std::string &symbol, double fee_rate) {
  // Record fill in history
  trade_history.push_back(fill);

  // Calculate fee
  double notional = fill.price * fill.quantity;
  double fee = notional * fee_rate;
  total_fees_paid += fee;

  // Update cash balance
  if (side == Side::BUY) {
    cash_balance -= (notional + fee); // Pay for shares + fee
  } else {
    cash_balance += (notional - fee); // Receive cash - fee
  }

  // Update position
  update_position_on_fill(fill, side, symbol);

  total_trades++;
}

void Account::update_position_on_fill(const Fill &fill, Side side,
                                      const std::string &symbol) {
  // Get or create position for this symbol
  if (positions.find(symbol) == positions.end()) {
    positions[symbol] = Position(symbol);
  }

  Position &pos = positions[symbol];
  int fill_qty = fill.quantity;
  double fill_price = fill.price;

  // Determine signed quantity (positive for buy, negative for sell)
  int signed_qty = (side == Side::BUY) ? fill_qty : -fill_qty;

  // Previous position state
  int old_qty = pos.quantity;
  double old_avg_price = pos.average_price;

  if (old_qty == 0) {
    // Opening new position
    pos.quantity = signed_qty;
    pos.average_price = fill_price;
    pos.total_cost_basis = std::abs(signed_qty) * fill_price;
  } else if ((old_qty > 0 && signed_qty > 0) ||
             (old_qty < 0 && signed_qty < 0)) {
    // Adding to existing position (same direction)
    int new_qty = old_qty + signed_qty;
    double new_cost_basis =
        pos.total_cost_basis + (std::abs(signed_qty) * fill_price);
    pos.total_cost_basis = new_cost_basis;
    pos.average_price = new_cost_basis / std::abs(new_qty);
    pos.quantity = new_qty;
  } else {
    // Reducing or reversing position (opposite direction)
    int abs_old_qty = std::abs(old_qty);
    int abs_new_qty = std::abs(signed_qty);

    if (abs_new_qty <= abs_old_qty) {
      // Partially or fully closing position
      double exit_pnl;
      if (old_qty > 0) {
        // Closing long position (sold)
        exit_pnl = (fill_price - old_avg_price) * abs_new_qty;
      } else {
        // Closing short position (bought)
        exit_pnl = (old_avg_price - fill_price) * abs_new_qty;
      }

      pos.realized_pnl += exit_pnl;
      pos.quantity += signed_qty; // Reduce position

      // Update statistics
      update_statistics(exit_pnl);

      // Update cost basis proportionally
      if (pos.quantity == 0) {
        pos.average_price = 0.0;
        pos.total_cost_basis = 0.0;
      } else {
        double reduction_ratio = static_cast<double>(abs_new_qty) / abs_old_qty;
        pos.total_cost_basis *= (1.0 - reduction_ratio);
      }
    } else {
      // Reversing position (closing all and opening opposite)
      double exit_pnl;
      if (old_qty > 0) {
        exit_pnl = (fill_price - old_avg_price) * abs_old_qty;
      } else {
        exit_pnl = (old_avg_price - fill_price) * abs_old_qty;
      }

      pos.realized_pnl += exit_pnl;
      update_statistics(exit_pnl);

      // New position in opposite direction
      int remaining_qty = abs_new_qty - abs_old_qty;
      pos.quantity = (signed_qty > 0) ? remaining_qty : -remaining_qty;
      pos.average_price = fill_price;
      pos.total_cost_basis = remaining_qty * fill_price;
    }
  }
}

void Account::update_statistics(double pnl) {
  if (pnl > 0) {
    winning_trades++;
    gross_profit += pnl;
  } else if (pnl < 0) {
    losing_trades++;
    gross_loss += std::abs(pnl);
  }
}

double Account::calculate_total_pnl(
    const std::unordered_map<std::string, double> &current_prices) const {
  double total_pnl = get_total_realized_pnl();

  // Add unrealized P&L for each open position
  for (const auto &[symbol, pos] : positions) {
    auto it = current_prices.find(symbol);
    if (it != current_prices.end() && pos.quantity != 0) {
      double current_price = it->second;
      double unrealized = (current_price - pos.average_price) * pos.quantity;
      total_pnl += unrealized;
    }
  }

  return total_pnl;
}

double Account::calculate_account_value(
    const std::unordered_map<std::string, double> &current_prices) const {
  double value = cash_balance;

  // Add market value of all positions
  for (const auto &[symbol, pos] : positions) {
    auto it = current_prices.find(symbol);
    if (it != current_prices.end() && pos.quantity != 0) {
      double current_price = it->second;
      value += current_price * pos.quantity;
    }
  }

  return value;
}

double Account::get_total_realized_pnl() const {
  double total = 0.0;
  for (const auto &[symbol, pos] : positions) {
    total += pos.realized_pnl;
  }
  return total;
}

double Account::get_total_unrealized_pnl() const {
  double total = 0.0;
  for (const auto &[symbol, pos] : positions) {
    total += pos.unrealized_pnl;
  }
  return total;
}

double Account::get_leverage(
    const std::unordered_map<std::string, double> &current_prices) const {
  double account_value = calculate_account_value(current_prices);
  if (account_value <= 0)
    return 0.0;

  double total_exposure = 0.0;
  for (const auto &[symbol, pos] : positions) {
    auto it = current_prices.find(symbol);
    if (it != current_prices.end()) {
      double current_price = it->second;
      total_exposure += std::abs(pos.quantity * current_price);
    }
  }

  return total_exposure / account_value;
}

double Account::get_margin_used(
    const std::unordered_map<std::string, double> &current_prices) const {
  double margin = 0.0;
  for (const auto &[symbol, pos] : positions) {
    auto it = current_prices.find(symbol);
    if (it != current_prices.end()) {
      double current_price = it->second;
      // Simplified: assume 100% margin requirement
      margin += std::abs(pos.quantity * current_price);
    }
  }
  return margin;
}

double Account::get_win_rate() const {
  if (total_trades == 0)
    return 0.0;
  return (static_cast<double>(winning_trades) / total_trades) * 100.0;
}

double Account::get_profit_factor() const {
  if (gross_loss == 0.0)
    return (gross_profit > 0) ? std::numeric_limits<double>::infinity() : 0.0;
  return gross_profit / gross_loss;
}

double Account::get_average_win() const {
  if (winning_trades == 0)
    return 0.0;
  return gross_profit / winning_trades;
}

double Account::get_average_loss() const {
  if (losing_trades == 0)
    return 0.0;
  return gross_loss / losing_trades;
}

double Account::get_return_on_capital() const {
  if (initial_cash == 0.0)
    return 0.0;
  double total_pnl = get_total_realized_pnl() - total_fees_paid;
  return (total_pnl / initial_cash) * 100.0;
}

void Account::print_summary(
    const std::unordered_map<std::string, double> &current_prices) const {
  std::cout << "\n=== Account Summary: " << name << " (ID: " << account_id
            << ") ===" << std::endl;
  std::cout << std::string(70, '=') << std::endl;

  double account_value = calculate_account_value(current_prices);
  double total_pnl = calculate_total_pnl(current_prices);
  double realized_pnl = get_total_realized_pnl();
  double unrealized_pnl = total_pnl - realized_pnl;

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Initial Capital:     $" << std::setw(12) << initial_cash
            << std::endl;
  std::cout << "Cash Balance:        $" << std::setw(12) << cash_balance
            << std::endl;
  std::cout << "Account Value:       $" << std::setw(12) << account_value
            << std::endl;
  std::cout << std::string(70, '-') << std::endl;

  std::cout << "Realized P&L:        $" << std::setw(12) << realized_pnl
            << std::endl;
  std::cout << "Unrealized P&L:      $" << std::setw(12) << unrealized_pnl
            << std::endl;
  std::cout << "Total P&L:           $" << std::setw(12) << total_pnl
            << std::endl;
  std::cout << "Total Fees:          $" << std::setw(12) << total_fees_paid
            << std::endl;
  std::cout << "Net P&L:             $" << std::setw(12)
            << (total_pnl - total_fees_paid) << std::endl;

  std::cout << std::string(70, '-') << std::endl;
  std::cout << "Return on Capital:   " << std::setw(11)
            << get_return_on_capital() << "%" << std::endl;
  std::cout << "Leverage:            " << std::setw(12) << std::setprecision(2)
            << get_leverage(current_prices) << "x" << std::endl;
  std::cout << std::string(70, '=') << std::endl;
}

void Account::print_positions(
    const std::unordered_map<std::string, double> &current_prices) const {
  std::cout << "\n=== Open Positions ===" << std::endl;

  if (positions.empty() ||
      std::all_of(positions.begin(), positions.end(),
                  [](const auto &p) { return p.second.is_flat(); })) {
    std::cout << "No open positions." << std::endl;
    return;
  }

  std::cout << std::string(100, '-') << std::endl;
  std::cout << std::left << std::setw(10) << "Symbol" << std::right
            << std::setw(12) << "Quantity" << std::setw(15) << "Avg Price"
            << std::setw(15) << "Current Price" << std::setw(15)
            << "Unrealized P&L" << std::setw(15) << "Realized P&L"
            << std::setw(18) << "Total P&L" << std::endl;
  std::cout << std::string(100, '-') << std::endl;

  std::cout << std::fixed << std::setprecision(2);
  for (const auto &[symbol, pos] : positions) {
    if (pos.is_flat())
      continue;

    auto it = current_prices.find(symbol);
    double current_price = (it != current_prices.end()) ? it->second : 0.0;
    double unrealized = (current_price - pos.average_price) * pos.quantity;

    std::cout << std::left << std::setw(10) << symbol << std::right
              << std::setw(12) << pos.quantity << std::setw(15)
              << pos.average_price << std::setw(15) << current_price
              << std::setw(15) << unrealized << std::setw(15)
              << pos.realized_pnl << std::setw(18)
              << (unrealized + pos.realized_pnl) << std::endl;
  }
  std::cout << std::string(100, '-') << std::endl;
}

void Account::print_trade_history() const {
  std::cout << "\n=== Trade History (Last 20) ===" << std::endl;

  if (trade_history.empty()) {
    std::cout << "No trades yet." << std::endl;
    return;
  }

  size_t start = (trade_history.size() > 20) ? trade_history.size() - 20 : 0;

  std::cout << std::fixed << std::setprecision(2);
  for (size_t i = start; i < trade_history.size(); ++i) {
    const auto &fill = trade_history[i];
    std::cout << "[" << (i + 1) << "] " << fill << std::endl;
  }
}

void Account::print_performance_metrics() const {
  std::cout << "\n=== Performance Metrics ===" << std::endl;
  std::cout << std::string(50, '-') << std::endl;

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Total Trades:        " << total_trades << std::endl;
  std::cout << "Winning Trades:      " << winning_trades << std::endl;
  std::cout << "Losing Trades:       " << losing_trades << std::endl;
  std::cout << "Win Rate:            " << get_win_rate() << "%" << std::endl;
  std::cout << "Profit Factor:       " << get_profit_factor() << std::endl;
  std::cout << "Average Win:         $" << get_average_win() << std::endl;
  std::cout << "Average Loss:        $" << get_average_loss() << std::endl;
  std::cout << "Gross Profit:        $" << gross_profit << std::endl;
  std::cout << "Gross Loss:          $" << gross_loss << std::endl;
  std::cout << std::string(50, '-') << std::endl;
}