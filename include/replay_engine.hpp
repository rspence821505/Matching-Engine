#pragma once

#include "event.hpp"
#include "order_book.hpp"
#include <chrono>
#include <string>
#include <thread>
#include <vector>

class ReplayEngine {
private:
  OrderBook book_;
  std::vector<OrderEvent> events_;
  size_t current_idx_; // Current position in event stream

  // Statistics
  TimePoint replay_start_time_;
  size_t events_processed_;
  size_t fills_generated_;

public:
  ReplayEngine();

  // Load events from file
  void load_from_file(const std::string &filename);

  // Replay modes
  void replay_instant();                            // As fast as possible
  void replay_timed(double speed_multiplier = 1.0); // Time-accurate
  void replay_step_by_step();                       // Interactive stepping

  // Manual control using current_idx_
  bool has_next_event() const;
  void replay_next_event();       // Process one event
  void replay_n_events(size_t n); // Process N events
  void reset_replay();            // Reset to beginning
  void skip_to_event(size_t idx); // Jump to specific event

  // Query current state
  size_t get_current_index() const { return current_idx_; }
  size_t get_total_events() const { return events_.size(); }
  double get_progress_percentage() const;
  const OrderEvent &peek_next_event() const;

  // Validation
  void validate_against_original(const std::vector<Fill> &original_fills);

  // Access results
  const OrderBook &get_book() const { return book_; }
  OrderBook &get_book_mutable() { return book_; } // For testing
  void print_replay_summary() const;

private:
  void replay_event(const OrderEvent &event);
  void print_progress(size_t current, size_t total);
};
