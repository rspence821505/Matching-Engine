#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// project headers
#include "fill.hpp"
#include "types.hpp"

struct Position {
  std::string symbol;
  int quantity;            // Net position (positive = long, negative = short)
  double average_price;    // Volume-weighted average entry price
  double realized_pnl;     // Locked-in P&L from closed trades
  double unrealized_pnl;   // Mark-to-market P&L on open position
  double total_cost_basis; // Total cost of position (for VWAP calculation)

  Position()
      : symbol(""), quantity(0), average_price(0.0), realized_pnl(0.0),
        unrealized_pnl(0.0), total_cost_basis(0.0) {}

  Position(const std::string &sym)
      : symbol(sym), quantity(0), average_price(0.0), realized_pnl(0.0),
        unrealized_pnl(0.0), total_cost_basis(0.0) {}

  bool is_flat() const { return quantity == 0; }
  bool is_long() const { return quantity > 0; }
  bool is_short() const { return quantity < 0; }

  void update_unrealized_pnl(double current_price) {
    if (quantity == 0) {
      unrealized_pnl = 0.0;
      return;
    }
    unrealized_pnl = (current_price - average_price) * quantity;
  }
};

struct Account {
  int account_id;
  std::string name;
  double initial_cash;
  double cash_balance;
  double total_fees_paid;
  std::unordered_map<std::string, Position> positions; // symbol -> position
  std::vector<Fill> trade_history;

  // Statistics
  int total_trades;
  int winning_trades;
  int losing_trades;
  double gross_profit;
  double gross_loss;

  Account(int id, const std::string &account_name, double initial_balance)
      : account_id(id), name(account_name), initial_cash(initial_balance),
        cash_balance(initial_balance), total_fees_paid(0.0), total_trades(0),
        winning_trades(0), losing_trades(0), gross_profit(0.0),
        gross_loss(0.0) {}

  // Update account on a fill
  void process_fill(const Fill &fill, Side side, const std::string &symbol,
                    double fee_rate = 0.0001);

  // Calculate total P&L (realized + unrealized)
  double calculate_total_pnl(
      const std::unordered_map<std::string, double> &current_prices) const;

  // Calculate total account value (cash + positions at market)
  double calculate_account_value(
      const std::unordered_map<std::string, double> &current_prices) const;

  // Get realized P&L across all positions
  double get_total_realized_pnl() const;

  // Get unrealized P&L across all positions
  double get_total_unrealized_pnl() const;

  // Risk metrics
  double get_leverage(
      const std::unordered_map<std::string, double> &current_prices) const;
  double get_margin_used(
      const std::unordered_map<std::string, double> &current_prices) const;

  // Performance metrics
  double get_win_rate() const;
  double get_profit_factor() const;
  double get_average_win() const;
  double get_average_loss() const;
  double get_return_on_capital() const;

  // Display
  void print_summary(
      const std::unordered_map<std::string, double> &current_prices) const;
  void print_positions(
      const std::unordered_map<std::string, double> &current_prices) const;
  void print_trade_history() const;
  void print_performance_metrics() const;

private:
  void update_position_on_fill(const Fill &fill, Side side,
                               const std::string &symbol);
  void update_statistics(double pnl);
};
