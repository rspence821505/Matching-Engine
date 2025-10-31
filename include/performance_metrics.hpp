#pragma once

#include <cstddef>  // for size_t (optional but good practice)
#include <iomanip>  // for std::setprecision, formatting output
#include <iostream> // for std::cout in print_report
#include <utility>  // for std::pair
#include <vector>   // for std::vector

// project headers
#include "account.hpp" // for Account
#include "timer.hpp"   // for TimePoint

struct PerformanceMetrics {
  std::vector<std::pair<TimePoint, double>> pnl_timeseries;
  double sharpe_ratio;
  double max_drawdown;
  double total_fees_paid;
  int total_trades;
  double win_rate;

  void calculate(const std::vector<Account> &accounts);
  void print_report() const;
};