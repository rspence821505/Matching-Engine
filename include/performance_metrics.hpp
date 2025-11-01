#pragma once

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// project headers
#include "account.hpp" // for Account
#include "timer.hpp"   // for TimePoint

struct PerformanceMetrics {
  // Time series data
  std::vector<std::pair<TimePoint, double>> pnl_timeseries;

  // Core metrics
  double sharpe_ratio;
  double max_drawdown;
  double total_fees_paid;
  int total_trades;
  double win_rate;

  // ==================================================================
  // PRIMARY INTERFACE
  // ==================================================================

  // Calculate all metrics from account data
  void calculate(const std::vector<Account> &accounts);

  // Print comprehensive report
  void print_report() const;

  // ==================================================================
  // TIMESERIES MANAGEMENT
  // ==================================================================

  // Add a P&L snapshot to the timeseries
  void add_pnl_snapshot(TimePoint timestamp, double pnl);

  // Clear timeseries data
  void clear_timeseries();

  // Export timeseries to CSV
  void export_to_csv(const std::string &filename) const;

  // ==================================================================
  // ADDITIONAL METRICS
  // ==================================================================

  // Get total return (dollar amount)
  double get_total_return() const;

  // Get return as percentage
  double get_return_percentage() const;

  // Calculate Calmar ratio (return / max drawdown)
  double get_calmar_ratio() const;

  // Calculate Sortino ratio (like Sharpe but only penalizes downside)
  double get_sortino_ratio() const;

  // Get return mean and standard deviation
  std::pair<double, double> get_return_statistics() const;

  // Print advanced metrics
  void print_advanced_metrics() const;

private:
  // ==================================================================
  // INTERNAL CALCULATION HELPERS
  // ==================================================================

  // Calculate Sharpe ratio from timeseries
  double calculate_sharpe_ratio() const;

  // Calculate maximum drawdown from timeseries
  double calculate_max_drawdown() const;
};