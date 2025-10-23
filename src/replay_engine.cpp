#include "replay_engine.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>

ReplayEngine::ReplayEngine()
    : current_idx_(0), events_processed_(0), fills_generated_(0) {}

void ReplayEngine::load_from_file(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }

  std::string line;
  std::getline(file, line); // Skip header

  events_.clear();
  current_idx_ = 0; // Reset position

  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    events_.push_back(OrderEvent::from_csv(line));
  }

  file.close();
  std::cout << "Loaded " << events_.size() << " events from " << filename
            << std::endl;
}

void ReplayEngine::replay_instant() {
  std::cout << "\n Starting INSTANT replay..." << std::endl;
  replay_start_time_ = Clock::now();

  reset_replay(); // Start from beginning

  while (has_next_event()) {
    replay_next_event();

    if ((current_idx_ % 1000 == 0) || !has_next_event()) {
      print_progress(current_idx_, events_.size());
    }
  }

  print_replay_summary();
}

void ReplayEngine::replay_timed(double speed_multiplier) {
  if (events_.empty()) {
    std::cout << "No events to replay!" << std::endl;
    return;
  }

  std::cout << "\nStarting TIMED replay at " << speed_multiplier << "x speed..."
            << std::endl;
  replay_start_time_ = Clock::now();

  reset_replay(); // Start from beginning

  TimePoint last_event_time = events_[0].timestamp;
  bool first = true;

  while (has_next_event()) {
    const auto &event = peek_next_event();

    if (!first && speed_multiplier > 0) {
      auto event_gap = event.timestamp - last_event_time;
      auto scaled_gap =
          std::chrono::duration_cast<std::chrono::nanoseconds>(event_gap) /
          speed_multiplier;
      std::this_thread::sleep_for(scaled_gap);
    }

    last_event_time = event.timestamp;
    first = false;

    replay_next_event();

    if ((current_idx_ % 100 == 0) || !has_next_event()) {
      print_progress(current_idx_, events_.size());
    }
  }

  print_replay_summary();
}

void ReplayEngine::replay_step_by_step() {
  std::cout << "\nStarting STEP-BY-STEP replay..." << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  ENTER     - Execute next event" << std::endl;
  std::cout << "  n <num>   - Execute next N events" << std::endl;
  std::cout << "  j <num>   - Jump to event number" << std::endl;
  std::cout << "  p         - Print current book state" << std::endl;
  std::cout << "  r         - Reset to beginning" << std::endl;
  std::cout << "  q         - Quit" << std::endl;
  std::cout << std::endl;

  replay_start_time_ = Clock::now();
  reset_replay();

  while (has_next_event()) {
    const auto &event = peek_next_event();

    std::cout << "\n[" << (current_idx_ + 1) << "/" << events_.size() << "] "
              << event.to_string() << std::endl;

    std::cout << "> ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
      // Just ENTER - execute next
      replay_next_event();
      book_.print_top_of_book();
    } else if (input == "q" || input == "quit") {
      std::cout << "Replay stopped at event " << current_idx_ << std::endl;
      break;
    } else if (input == "p" || input == "print") {
      book_.print_market_depth(5);
    } else if (input == "r" || input == "reset") {
      reset_replay();
      std::cout << "Reset to beginning" << std::endl;
    } else if (input[0] == 'n' && input.length() > 2) {
      // Execute N events
      int n = std::stoi(input.substr(2));
      replay_n_events(n);
      book_.print_top_of_book();
    } else if (input[0] == 'j' && input.length() > 2) {
      // Jump to event
      int target = std::stoi(input.substr(2)) - 1; // 0-indexed
      if (target >= 0 && target < static_cast<int>(events_.size())) {
        skip_to_event(target);
        std::cout << "Jumped to event " << (current_idx_ + 1) << std::endl;
      } else {
        std::cout << "Invalid event number" << std::endl;
      }
    } else {
      std::cout << "Unknown command. Press ENTER to continue, 'q' to quit."
                << std::endl;
    }
  }

  print_replay_summary();
}

// Manual control methods using current_idx_
bool ReplayEngine::has_next_event() const {
  return current_idx_ < events_.size();
}

void ReplayEngine::replay_next_event() {
  if (!has_next_event()) {
    throw std::runtime_error("No more events to replay");
  }

  replay_event(events_[current_idx_]);
  current_idx_++;
}

void ReplayEngine::replay_n_events(size_t n) {
  size_t target = std::min(current_idx_ + n, events_.size());

  while (current_idx_ < target) {
    replay_next_event();
  }

  std::cout << "Executed " << n << " events (now at " << current_idx_ << "/"
            << events_.size() << ")" << std::endl;
}

void ReplayEngine::reset_replay() {
  current_idx_ = 0;
  events_processed_ = 0;
  fills_generated_ = 0;

  // Clear order book
  book_ = OrderBook(); // Reset to fresh state
}

void ReplayEngine::skip_to_event(size_t idx) {
  if (idx >= events_.size()) {
    throw std::runtime_error("Event index out of range");
  }

  if (idx < current_idx_) {
    // Need to reset and replay from start
    reset_replay();
  }

  // Replay up to target index
  while (current_idx_ < idx) {
    replay_next_event();
  }
}

double ReplayEngine::get_progress_percentage() const {
  if (events_.empty())
    return 0.0;
  return (current_idx_ * 100.0) / events_.size();
}

const OrderEvent &ReplayEngine::peek_next_event() const {
  if (!has_next_event()) {
    throw std::runtime_error("No more events to peek");
  }
  return events_[current_idx_];
}

// Rest of the implementation remains the same...
void ReplayEngine::replay_event(const OrderEvent &event) {
  switch (event.type) {
  case EventType::NEW_ORDER:
    if (event.peak_size > 0) {
      book_.add_order(Order{event.order_id, event.side, event.price,
                            event.quantity, event.peak_size, event.tif});
    } else if (event.order_type == OrderType::MARKET) {
      book_.add_order(Order{event.order_id, event.side, event.order_type,
                            event.quantity, event.tif});
    } else {
      book_.add_order(Order{event.order_id, event.side, event.price,
                            event.quantity, event.tif});
    }
    break;

  case EventType::CANCEL_ORDER:
    book_.cancel_order(event.order_id);
    break;

  case EventType::AMEND_ORDER: {
    std::optional<double> new_price;
    std::optional<int> new_qty;
    if (event.has_new_price)
      new_price = event.new_price;
    if (event.has_new_quantity)
      new_qty = event.new_quantity;
    book_.amend_order(event.order_id, new_price, new_qty);
    break;
  }

  case EventType::FILL:
    fills_generated_++;
    break;
  }

  events_processed_++;
}

void ReplayEngine::validate_against_original(
    const std::vector<Fill> &original_fills) {
  const auto &replay_fills = book_.get_fills();

  std::cout << "\n=== Replay Validation ===" << std::endl;
  std::cout << "Original fills: " << original_fills.size() << std::endl;
  std::cout << "Replay fills:   " << replay_fills.size() << std::endl;

  if (original_fills.size() != replay_fills.size()) {
    std::cout << "FAIL: Fill count mismatch!" << std::endl;
    return;
  }

  bool all_match = true;
  for (size_t i = 0; i < original_fills.size(); ++i) {
    const auto &orig = original_fills[i];
    const auto &replay = replay_fills[i];

    if (orig.buy_order_id != replay.buy_order_id ||
        orig.sell_order_id != replay.sell_order_id ||
        std::abs(orig.price - replay.price) > 0.0001 ||
        orig.quantity != replay.quantity) {

      std::cout << "MISMATCH at fill " << i << ":" << std::endl;
      std::cout << "  Original: " << orig << std::endl;
      std::cout << "  Replay:   " << replay << std::endl;
      all_match = false;
    }
  }

  if (all_match) {
    std::cout << "SUCCESS: All fills match perfectly!" << std::endl;
  }
}

void ReplayEngine::print_progress(size_t current, size_t total) {
  double pct = (current * 100.0) / total;
  std::cout << "\rProgress: " << current << "/" << total << " (" << std::fixed
            << std::setprecision(1) << pct << "%)" << std::flush;
}

void ReplayEngine::print_replay_summary() const {
  auto replay_duration = Clock::now() - replay_start_time_;
  auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(replay_duration)
          .count();

  std::cout << "\n\n=== Replay Summary ===" << std::endl;
  std::cout << "Events processed: " << events_processed_ << std::endl;
  std::cout << "Current position: " << current_idx_ << "/" << events_.size()
            << std::endl;
  std::cout << "Fills generated:  " << book_.get_fills().size() << std::endl;
  std::cout << "Replay time:      " << duration_ms << " ms" << std::endl;

  if (duration_ms > 0) {
    double eps = (events_processed_ * 1000.0) / duration_ms;
    std::cout << "Throughput:       " << std::fixed << std::setprecision(0)
              << eps << " events/sec" << std::endl;
  }

  std::cout << std::endl;
  book_.print_book_summary();
}