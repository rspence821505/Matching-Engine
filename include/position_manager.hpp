#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// project headers
#include "account.hpp" // defines Account
#include "fill.hpp"    // defines Fill

class PositionManager {
private:
  std::unordered_map<int, std::unique_ptr<Account>> accounts_;
  std::unordered_map<std::string, double>
      current_prices_; // symbol -> last price
  double default_fee_rate_;

  // Risk limits
  struct RiskLimits {
    double max_position_size; // Maximum position size per symbol
    double max_loss_per_day;  // Maximum daily loss allowed
    double max_leverage;      // Maximum leverage allowed
    bool enabled;

    RiskLimits()
        : max_position_size(1000000), max_loss_per_day(50000),
          max_leverage(3.0), enabled(false) {}
  };

  std::unordered_map<int, RiskLimits> account_limits_;

public:
  PositionManager(double fee_rate = 0.0001);

  // Account management
  void create_account(int account_id, const std::string &name,
                      double initial_cash);
  bool has_account(int account_id) const;
  Account &get_account(int account_id);
  const Account &get_account(int account_id) const;
  std::vector<int> get_all_account_ids() const;

  // Trade processing
  void process_fill(const Fill &fill, int buy_account_id, int sell_account_id,
                    const std::string &symbol);

  // Price updates
  void update_price(const std::string &symbol, double price);
  void update_prices(const std::unordered_map<std::string, double> &prices);
  double get_current_price(const std::string &symbol) const;
  const std::unordered_map<std::string, double> &get_current_prices() const {
    return current_prices_;
  }

  // Risk management
  void set_risk_limits(int account_id, double max_position, double max_loss,
                       double max_leverage);
  void enable_risk_limits(int account_id);
  void disable_risk_limits(int account_id);
  bool check_risk_limits(int account_id, const std::string &symbol,
                         int quantity, double price) const;

  // Reporting
  void print_account_summary(int account_id) const;
  void print_all_accounts() const;
  void print_positions_summary() const;
  void print_aggregate_statistics() const;

  // Aggregate metrics across all accounts
  double get_total_account_value() const;
  double get_total_pnl() const;
  double get_total_fees_paid() const;
  int get_total_trades() const;

  // Export
  void export_account_summary(int account_id,
                              const std::string &filename) const;
  void export_all_accounts(const std::string &filename) const;

  // Reset (for testing/simulation)
  void reset();
  void reset_account(int account_id);

private:
  void validate_account_exists(int account_id) const;
};
