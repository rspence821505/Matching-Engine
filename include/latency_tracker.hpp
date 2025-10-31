#pragma once

#include <vector>

class LatencyTracker {
private:
  std::vector<long long> latencies_;

  long long percentile(double p);
  void print_histogram();

public:
  void record(long long latency_ns);
  void print_statistics();
};
