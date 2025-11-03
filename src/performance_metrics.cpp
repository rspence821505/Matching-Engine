#include "performance_metrics.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

void PerformanceMetrics::calculate(const std::vector<Account> &accounts) {
  // Reset all metrics
  total_trades = 0;
  total_fees_paid = 0;
  win_rate = 0.0;
  sharpe_ratio = 0.0;
  max_drawdown = 0.0;

  // Aggregate basic statistics across all accounts (if provided)
  int total_wins = 0;
  int total_losses = 0;

  for (const auto &account : accounts) {
    total_trades += account.total_trades;
    total_fees_paid += account.total_fees_paid;
    total_wins += account.winning_trades;
    total_losses += account.losing_trades;
  }
  (void)total_losses;

  // Calculate win rate
  if (total_trades > 0) {
    win_rate = (static_cast<double>(total_wins) / total_trades) * 100.0;
  }

  // Calculate metrics from P&L timeseries if available
  if (!pnl_timeseries.empty()) {
    sharpe_ratio = calculate_sharpe_ratio();
    max_drawdown = calculate_max_drawdown();
  }
}

double PerformanceMetrics::calculate_sharpe_ratio() const {
  if (pnl_timeseries.size() < 2) {
    return 0.0;
  }

  // Calculate daily returns
  std::vector<double> returns;
  returns.reserve(pnl_timeseries.size() - 1);

  for (size_t i = 1; i < pnl_timeseries.size(); ++i) {
    double prev_pnl = pnl_timeseries[i - 1].second;
    double curr_pnl = pnl_timeseries[i].second;

    // Avoid division by zero
    if (std::abs(prev_pnl) < 1e-6) {
      returns.push_back(0.0);
    } else {
      double daily_return = (curr_pnl - prev_pnl) / std::abs(prev_pnl);
      returns.push_back(daily_return);
    }
  }

  if (returns.empty()) {
    return 0.0;
  }

  // Calculate mean return
  double mean_return =
      std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

  // Calculate standard deviation of returns
  double sum_squared_diff = 0.0;
  for (double ret : returns) {
    double diff = ret - mean_return;
    sum_squared_diff += diff * diff;
  }

  double stddev = std::sqrt(sum_squared_diff / returns.size());

  // Avoid division by zero
  if (stddev < 1e-10) {
    return 0.0;
  }

  // Annualized Sharpe ratio (assuming 252 trading days)
  // Sharpe = (mean_return / stddev) * sqrt(252)
  double sharpe = (mean_return / stddev) * std::sqrt(252.0);

  return sharpe;
}

double PerformanceMetrics::calculate_max_drawdown() const {
  if (pnl_timeseries.size() < 2) {
    return 0.0;
  }

  double max_drawdown = 0.0;
  double running_peak = pnl_timeseries[0].second;

  for (size_t i = 1; i < pnl_timeseries.size(); ++i) {
    double current_pnl = pnl_timeseries[i].second;

    // Update running peak if we have a new high
    if (current_pnl > running_peak) {
      running_peak = current_pnl;
    }

    // Calculate drawdown from peak
    double drawdown = 0.0;
    if (running_peak > 0) {
      drawdown = ((running_peak - current_pnl) / running_peak) * 100.0;
    }

    // Track maximum drawdown
    if (drawdown > max_drawdown) {
      max_drawdown = drawdown;
    }
  }

  return max_drawdown;
}

void PerformanceMetrics::print_report() const {
  std::cout
      << "\n╔════════════════════════════════════════════════════════════╗"
      << std::endl;
  std::cout << "║           PERFORMANCE METRICS REPORT                       ║"
            << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝"
            << std::endl;

  std::cout << "\n=== Trading Activity ===" << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << "Total Trades:        " << total_trades << std::endl;
  std::cout << "Win Rate:            " << std::fixed << std::setprecision(2)
            << win_rate << "%" << std::endl;

  std::cout << "\n=== Cost Analysis ===" << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << "Total Fees Paid:     $" << std::fixed << std::setprecision(2)
            << total_fees_paid << std::endl;

  std::cout << "\n=== Risk-Adjusted Performance ===" << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << "Sharpe Ratio:        " << std::fixed << std::setprecision(3)
            << sharpe_ratio;

  if (sharpe_ratio > 2.0) {
    std::cout << " (Excellent)";
  } else if (sharpe_ratio > 1.0) {
    std::cout << " (Good)";
  } else if (sharpe_ratio > 0.5) {
    std::cout << " (Acceptable)";
  } else if (sharpe_ratio > 0) {
    std::cout << " (Poor)";
  } else {
    std::cout << " (Negative)";
  }
  std::cout << std::endl;

  std::cout << "Maximum Drawdown:    " << std::fixed << std::setprecision(2)
            << max_drawdown << "%" << std::endl;

  if (!pnl_timeseries.empty()) {
    std::cout << "\n=== P&L Timeline ===" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    double initial_pnl = pnl_timeseries.front().second;
    double final_pnl = pnl_timeseries.back().second;
    double total_return = final_pnl - initial_pnl;
    double return_pct =
        (initial_pnl != 0.0)
            ? ((final_pnl - initial_pnl) / std::abs(initial_pnl)) * 100.0
            : 0.0;

    std::cout << "Initial P&L:         $" << std::fixed << std::setprecision(2)
              << initial_pnl << std::endl;
    std::cout << "Final P&L:           $" << std::fixed << std::setprecision(2)
              << final_pnl << std::endl;
    std::cout << "Total Return:        $" << std::fixed << std::setprecision(2)
              << total_return << " (" << return_pct << "%)" << std::endl;
    std::cout << "Data Points:         " << pnl_timeseries.size() << std::endl;
  }

  std::cout << "\n" << std::string(60, '=') << std::endl;

  // Interpretation guidance
  std::cout << "\n=== Metrics Interpretation ===" << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << "Sharpe Ratio:" << std::endl;
  std::cout << "  > 2.0   = Excellent (strong risk-adjusted returns)"
            << std::endl;
  std::cout << "  1.0-2.0 = Good (solid risk-adjusted returns)" << std::endl;
  std::cout << "  0.5-1.0 = Acceptable (moderate returns for risk)"
            << std::endl;
  std::cout << "  < 0.5   = Poor (low returns for risk taken)" << std::endl;
  std::cout << "\nMaximum Drawdown:" << std::endl;
  std::cout << "  Measures largest peak-to-trough decline" << std::endl;
  std::cout << "  Lower is better (< 10% is excellent, < 20% is good)"
            << std::endl;
  std::cout << std::string(60, '=') << std::endl;
}

void PerformanceMetrics::add_pnl_snapshot(TimePoint timestamp, double pnl) {
  pnl_timeseries.push_back({timestamp, pnl});
}

void PerformanceMetrics::clear_timeseries() { pnl_timeseries.clear(); }

double PerformanceMetrics::get_total_return() const {
  if (pnl_timeseries.size() < 2) {
    return 0.0;
  }

  double initial = pnl_timeseries.front().second;
  double final = pnl_timeseries.back().second;

  return final - initial;
}

double PerformanceMetrics::get_return_percentage() const {
  if (pnl_timeseries.size() < 2) {
    return 0.0;
  }

  double initial = pnl_timeseries.front().second;
  double final = pnl_timeseries.back().second;

  if (std::abs(initial) < 1e-6) {
    return 0.0;
  }

  return ((final - initial) / std::abs(initial)) * 100.0;
}

double PerformanceMetrics::get_calmar_ratio() const {
  if (max_drawdown < 1e-6) {
    return 0.0; // Undefined if no drawdown
  }

  double total_return_pct = get_return_percentage();

  // Calmar Ratio = Annual Return / Max Drawdown
  // For simplicity, we'll use total return percentage
  return total_return_pct / max_drawdown;
}

double PerformanceMetrics::get_sortino_ratio() const {
  if (pnl_timeseries.size() < 2) {
    return 0.0;
  }

  // Calculate daily returns
  std::vector<double> returns;
  returns.reserve(pnl_timeseries.size() - 1);

  for (size_t i = 1; i < pnl_timeseries.size(); ++i) {
    double prev = pnl_timeseries[i - 1].second;
    double curr = pnl_timeseries[i].second;

    if (std::abs(prev) > 1e-6) {
      returns.push_back((curr - prev) / std::abs(prev));
    }
  }

  if (returns.empty()) {
    return 0.0;
  }

  // Mean return
  double mean_return =
      std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

  // Downside deviation (only negative returns)
  double sum_squared_negative = 0.0;
  int negative_count = 0;

  for (double ret : returns) {
    if (ret < 0) {
      sum_squared_negative += ret * ret;
      negative_count++;
    }
  }

  if (negative_count == 0) {
    return 0.0; // No negative returns
  }

  double downside_dev = std::sqrt(sum_squared_negative / negative_count);

  if (downside_dev < 1e-10) {
    return 0.0;
  }

  // Annualized Sortino ratio
  return (mean_return / downside_dev) * std::sqrt(252.0);
}

void PerformanceMetrics::print_advanced_metrics() const {
  std::cout << "\n=== Advanced Performance Metrics ===" << std::endl;
  std::cout << std::string(60, '-') << std::endl;

  std::cout << "Calmar Ratio:        " << std::fixed << std::setprecision(3)
            << get_calmar_ratio() << std::endl;
  std::cout << "  (Return / Max Drawdown - higher is better)" << std::endl;

  std::cout << "\nSortino Ratio:       " << std::fixed << std::setprecision(3)
            << get_sortino_ratio() << std::endl;
  std::cout << "  (Like Sharpe, but only penalizes downside volatility)"
            << std::endl;

  if (pnl_timeseries.size() >= 2) {
    std::cout << "\nTotal Return:        " << std::fixed << std::setprecision(2)
              << get_return_percentage() << "%" << std::endl;
  }

  std::cout << std::string(60, '-') << std::endl;
}

std::pair<double, double> PerformanceMetrics::get_return_statistics() const {
  if (pnl_timeseries.size() < 2) {
    return {0.0, 0.0};
  }

  std::vector<double> returns;
  for (size_t i = 1; i < pnl_timeseries.size(); ++i) {
    double prev = pnl_timeseries[i - 1].second;
    double curr = pnl_timeseries[i].second;

    if (std::abs(prev) > 1e-6) {
      returns.push_back((curr - prev) / std::abs(prev));
    }
  }

  if (returns.empty()) {
    return {0.0, 0.0};
  }

  // Mean
  double mean =
      std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

  // Standard deviation
  double sum_sq = 0.0;
  for (double r : returns) {
    double diff = r - mean;
    sum_sq += diff * diff;
  }
  double stddev = std::sqrt(sum_sq / returns.size());

  return {mean, stddev};
}

void PerformanceMetrics::export_to_csv(const std::string &filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file for writing: " + filename);
  }

  // Write header
  file << "timestamp,pnl,cumulative_return\n";

  if (pnl_timeseries.empty()) {
    file.close();
    return;
  }

  double initial_pnl = pnl_timeseries[0].second;

  for (const auto &[timestamp, pnl] : pnl_timeseries) {
    double cumulative_return = 0.0;
    if (std::abs(initial_pnl) > 1e-6) {
      cumulative_return = ((pnl - initial_pnl) / std::abs(initial_pnl)) * 100.0;
    }

    file << timestamp.time_since_epoch().count() << "," << std::fixed
         << std::setprecision(2) << pnl << "," << cumulative_return << "\n";
  }

  file.close();
  std::cout << "Performance metrics exported to " << filename << std::endl;
}