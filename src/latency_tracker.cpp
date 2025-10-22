#include "latency_tracker.hpp"
#include <algorithm>
#include <climits>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

void LatencyTracker::record(long long latency_ns) {
  latencies_.push_back(latency_ns);
}

void LatencyTracker::print_statistics() {
  if (latencies_.empty()) {
    std::cout << "No latencies recorded!" << std::endl;
    return;
  }

  std::sort(latencies_.begin(), latencies_.end());

  std::cout << "\n=== Latency Distribution ===" << std::endl;
  std::cout << "p50 (median): " << percentile(50) << " ns" << std::endl;
  std::cout << "p95: " << percentile(95) << " ns" << std::endl;
  std::cout << "p99: " << percentile(99) << " ns" << std::endl;
  std::cout << "p99.9: " << percentile(99.9) << " ns" << std::endl;

  print_histogram();
}

long long LatencyTracker::percentile(double p) {
  size_t n = latencies_.size();
  size_t index = static_cast<size_t>((p / 100.0) * n);
  if (index >= n)
    index = n - 1;
  return latencies_[index];
}

void LatencyTracker::print_histogram() {
  std::cout << "\n=== Histogram ===" << std::endl;

  std::map<std::string, std::pair<long long, long long>> buckets = {
      {"<500ns", {0, 500}},
      {"500-750ns", {500, 750}},
      {"750-1000ns", {750, 1000}},
      {"1000-1500ns", {1000, 1500}},
      {">1500ns", {1500, LLONG_MAX}}};

  std::map<std::string, int> counts;
  for (const auto &[label, range] : buckets) {
    counts[label] = 0;
  }

  for (long long lat : latencies_) {
    for (const auto &[label, range] : buckets) {
      if (lat >= range.first && lat < range.second) {
        counts[label]++;
        break;
      }
    }
  }

  size_t total = latencies_.size();
  for (const auto &[label, range] : buckets) {
    int count = counts[label];
    double percentage = (count * 100.0) / total;

    std::cout << label << ": " << count << " (" << std::fixed
              << std::setprecision(1) << percentage << "%) ";

    int bar_length = static_cast<int>(percentage / 2);
    std::cout << std::string(bar_length, '#') << std::endl;
  }
}