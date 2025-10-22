#ifndef TYPES_HPP
#define TYPES_HPP

#include <chrono>

// Common type aliases
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

// Order side of book
enum class Side { BUY, SELL };

// Order type
enum class Type { NEW, CANCEL, AMEND };

// Order States to track order lifecycle
enum class OrderState {
  PENDING,          // Just created, not in book yet
  ACTIVE,           // In book, waiting to match
  PARTIALLY_FILLED, // Some quantity filled
  FILLED,           // Completely filled
  CANCELLED,        // Canceled by user
  REJECTED          // Rejected (e.g., invalid parameters)
};

#endif // TYPES_HPP