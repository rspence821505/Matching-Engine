#include "position_manager.hpp"
#include "order_book.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

PositionManager::PositionManager(double fee_rate)
    : default_fee_rate_(fee_rate) {}

void PositionManager::create_account(int account_id, const std::string &name,
                                     double initial_cash) {
  if (has_account(account_id)) {
    throw std::runtime_error("Account ID " + std::to_string(account_id) +
                             " already exists");
  }

  accounts_[account_id] =
      std::make_unique<Account>(account_id, name, initial_cash);

  std::cout << "Created account: " << name << " (ID: " << account_id
            << ") with $" << std::fixed << std::setprecision(2) << initial_cash
            << std::endl;
}

bool PositionManager::has_account(int account_id) const {
  return accounts_.find(account_id) != accounts_.end();
}

Account &PositionManager::get_account(int account_id) {
  validate_account_exists(account_id);
  return *accounts_.at(account_id);
}

const Account &PositionManager::get_account(int account_id) const {
  validate_account_exists(account_id);
  return *accounts_.at(account_id);
}

std::vector<int> PositionManager::get_all_account_ids() const {
  std::vector<int> ids;
  ids.reserve(accounts_.size());
  for (const auto &[id, account] : accounts_) {
    ids.push_back(id);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

void process_fills_from_orderbook(OrderBook &book, PositionManager &pos_mgr) {
  const auto &account_fills = book.get_account_fills();

  // Process only new fills (track last_processed index)
  static size_t last_processed = 0;

  for (size_t i = last_processed; i < account_fills.size(); ++i) {
    const auto &af = account_fills[i];

    // Automatically route to correct accounts!
    pos_mgr.process_fill(af.fill, af.buy_account_id, af.sell_account_id,
                         af.symbol);
  }

  last_processed = account_fills.size();
}

void PositionManager::process_fill(const Fill &fill, int buy_account_id,
                                   int sell_account_id,
                                   const std::string &symbol) {
  validate_account_exists(buy_account_id);
  validate_account_exists(sell_account_id);

  // Update current price
  update_price(symbol, fill.price);

  // Process for buyer
  Account &buyer = *accounts_[buy_account_id];
  buyer.process_fill(fill, Side::BUY, symbol, default_fee_rate_);

  // Process for seller
  Account &seller = *accounts_[sell_account_id];
  seller.process_fill(fill, Side::SELL, symbol, default_fee_rate_);
}

void PositionManager::update_price(const std::string &symbol, double price) {
  current_prices_[symbol] = price;

  // Update unrealized P&L for all accounts holding this symbol
  for (auto &[id, account] : accounts_) {
    auto it = account->positions.find(symbol);
    if (it != account->positions.end()) {
      it->second.update_unrealized_pnl(price);
    }
  }
}

void PositionManager::update_prices(
    const std::unordered_map<std::string, double> &prices) {
  for (const auto &[symbol, price] : prices) {
    update_price(symbol, price);
  }
}

double PositionManager::get_current_price(const std::string &symbol) const {
  auto it = current_prices_.find(symbol);
  if (it == current_prices_.end()) {
    return 0.0;
  }
  return it->second;
}

void PositionManager::set_risk_limits(int account_id, double max_position,
                                      double max_loss, double max_leverage) {
  validate_account_exists(account_id);

  RiskLimits limits;
  limits.max_position_size = max_position;
  limits.max_loss_per_day = max_loss;
  limits.max_leverage = max_leverage;
  limits.enabled = true;

  account_limits_[account_id] = limits;

  std::cout << "Risk limits set for account " << account_id << std::endl;
  std::cout << "  Max position size: " << max_position << std::endl;
  std::cout << "  Max daily loss: $" << max_loss << std::endl;
  std::cout << "  Max leverage: " << max_leverage << "x" << std::endl;
}

void PositionManager::enable_risk_limits(int account_id) {
  validate_account_exists(account_id);
  if (account_limits_.find(account_id) != account_limits_.end()) {
    account_limits_[account_id].enabled = true;
    std::cout << "Risk limits enabled for account " << account_id << std::endl;
  }
}

void PositionManager::disable_risk_limits(int account_id) {
  validate_account_exists(account_id);
  if (account_limits_.find(account_id) != account_limits_.end()) {
    account_limits_[account_id].enabled = false;
    std::cout << "Risk limits disabled for account " << account_id << std::endl;
  }
}

bool PositionManager::check_risk_limits(
    int account_id, [[maybe_unused]] const std::string &symbol, int quantity,
    double price) const {
  auto limits_it = account_limits_.find(account_id);
  if (limits_it == account_limits_.end() || !limits_it->second.enabled) {
    return true; // No limits or disabled
  }

  const RiskLimits &limits = limits_it->second;
  const Account &account = get_account(account_id);

  // Check position size limit
  double position_value = std::abs(quantity * price);
  if (position_value > limits.max_position_size) {
    std::cout << "Risk limit violation: Position size " << position_value
              << " exceeds limit " << limits.max_position_size << std::endl;
    return false;
  }

  // Check leverage limit
  double current_leverage = account.get_leverage(current_prices_);
  if (current_leverage > limits.max_leverage) {
    std::cout << "Risk limit violation: Leverage " << current_leverage
              << "x exceeds limit " << limits.max_leverage << "x" << std::endl;
    return false;
  }

  // Check max loss limit
  double total_pnl = account.calculate_total_pnl(current_prices_);
  if (total_pnl < -limits.max_loss_per_day) {
    std::cout << "Risk limit violation: Loss $" << std::abs(total_pnl)
              << " exceeds daily limit $" << limits.max_loss_per_day
              << std::endl;
    return false;
  }

  return true;
}

void PositionManager::print_account_summary(int account_id) const {
  validate_account_exists(account_id);
  const Account &account = *accounts_.at(account_id);
  account.print_summary(current_prices_);
  account.print_positions(current_prices_);
  account.print_performance_metrics();
}

void PositionManager::print_all_accounts() const {
  std::cout
      << "\n╔════════════════════════════════════════════════════════════╗"
      << std::endl;
  std::cout << "║           MULTI-ACCOUNT POSITION SUMMARY                   ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝"
            << std::endl;

  for (const auto &[id, account] : accounts_) {
    account->print_summary(current_prices_);
    std::cout << std::endl;
  }

  print_aggregate_statistics();
}

void PositionManager::print_positions_summary() const {
  std::cout << "\n=== All Open Positions ===" << std::endl;

  bool has_positions = false;
  for (const auto &[id, account] : accounts_) {
    bool account_has_positions = false;
    for (const auto &[symbol, pos] : account->positions) {
      if (!pos.is_flat()) {
        account_has_positions = true;
        has_positions = true;
        break;
      }
    }

    if (account_has_positions) {
      std::cout << "\nAccount: " << account->name << " (ID: " << id << ")"
                << std::endl;
      account->print_positions(current_prices_);
    }
  }

  if (!has_positions) {
    std::cout << "No open positions across all accounts." << std::endl;
  }
}

void PositionManager::print_aggregate_statistics() const {
  std::cout
      << "\n╔════════════════════════════════════════════════════════════╗"
      << std::endl;
  std::cout << "║              AGGREGATE STATISTICS                          ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝"
            << std::endl;

  double total_value = get_total_account_value();
  double total_pnl = get_total_pnl();
  double total_fees = get_total_fees_paid();
  int total_trades = get_total_trades();

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Number of Accounts:  " << accounts_.size() << std::endl;
  std::cout << "Total Account Value: $" << total_value << std::endl;
  std::cout << "Total P&L:           $" << total_pnl << std::endl;
  std::cout << "Total Fees Paid:     $" << total_fees << std::endl;
  std::cout << "Net P&L:             $" << (total_pnl - total_fees)
            << std::endl;
  std::cout << "Total Trades:        " << total_trades << std::endl;
  std::cout << std::string(60, '=') << std::endl;
}

double PositionManager::get_total_account_value() const {
  double total = 0.0;
  for (const auto &[id, account] : accounts_) {
    total += account->calculate_account_value(current_prices_);
  }
  return total;
}

double PositionManager::get_total_pnl() const {
  double total = 0.0;
  for (const auto &[id, account] : accounts_) {
    total += account->calculate_total_pnl(current_prices_);
  }
  return total;
}

double PositionManager::get_total_fees_paid() const {
  double total = 0.0;
  for (const auto &[id, account] : accounts_) {
    total += account->total_fees_paid;
  }
  return total;
}

int PositionManager::get_total_trades() const {
  int total = 0;
  for (const auto &[id, account] : accounts_) {
    total += account->total_trades;
  }
  return total;
}

void PositionManager::export_account_summary(
    int account_id, const std::string &filename) const {
  validate_account_exists(account_id);

  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }

  const Account &account = *accounts_.at(account_id);

  file << "Account Summary\n";
  file << "===============\n\n";
  file << "Account ID: " << account.account_id << "\n";
  file << "Name: " << account.name << "\n";
  file << "Initial Capital: $" << std::fixed << std::setprecision(2)
       << account.initial_cash << "\n";
  file << "Current Cash: $" << account.cash_balance << "\n";
  file << "Account Value: $" << account.calculate_account_value(current_prices_)
       << "\n";
  file << "Total P&L: $" << account.calculate_total_pnl(current_prices_)
       << "\n";
  file << "Return on Capital: " << account.get_return_on_capital() << "%\n\n";

  file << "Performance Metrics\n";
  file << "===================\n";
  file << "Total Trades: " << account.total_trades << "\n";
  file << "Win Rate: " << account.get_win_rate() << "%\n";
  file << "Profit Factor: " << account.get_profit_factor() << "\n\n";

  file << "Open Positions\n";
  file << "==============\n";
  for (const auto &[symbol, pos] : account.positions) {
    if (!pos.is_flat()) {
      file << symbol << ": " << pos.quantity << " @ $" << pos.average_price
           << " (Current: $" << get_current_price(symbol) << ")\n";
    }
  }

  file.close();
  std::cout << "Account summary exported to " << filename << std::endl;
}

void PositionManager::export_all_accounts(const std::string &filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }

  file << "Multi-Account Summary\n";
  file << "=====================\n\n";

  file << std::fixed << std::setprecision(2);
  for (const auto &[id, account] : accounts_) {
    file << "Account: " << account->name << " (ID: " << id << ")\n";
    file << "  Cash: $" << account->cash_balance << "\n";
    file << "  Value: $" << account->calculate_account_value(current_prices_)
         << "\n";
    file << "  P&L: $" << account->calculate_total_pnl(current_prices_) << "\n";
    file << "  Trades: " << account->total_trades << "\n\n";
  }

  file << "Aggregate Statistics\n";
  file << "====================\n";
  file << "Total Accounts: " << accounts_.size() << "\n";
  file << "Total Value: $" << get_total_account_value() << "\n";
  file << "Total P&L: $" << get_total_pnl() << "\n";
  file << "Total Trades: " << get_total_trades() << "\n";

  file.close();
  std::cout << "All accounts exported to " << filename << std::endl;
}

void PositionManager::reset() {
  accounts_.clear();
  current_prices_.clear();
  account_limits_.clear();
  std::cout << "Position manager reset complete." << std::endl;
}

void PositionManager::reset_account(int account_id) {
  validate_account_exists(account_id);

  Account &account = *accounts_[account_id];
  double initial_cash = account.initial_cash;
  std::string name = account.name;

  // Create fresh account
  accounts_[account_id] =
      std::make_unique<Account>(account_id, name, initial_cash);

  std::cout << "Account " << account_id << " has been reset." << std::endl;
}

void PositionManager::validate_account_exists(int account_id) const {
  if (!has_account(account_id)) {
    throw std::runtime_error("Account ID " + std::to_string(account_id) +
                             " does not exist");
  }
}
