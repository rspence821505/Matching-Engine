#ifndef LATENCY_TRACKER_HPP
#define LATENCY_TRACKER_HPP

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

#endif // LATENCY_TRACKER_HPP