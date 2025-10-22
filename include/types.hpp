#ifndef TYPES_HPP
#define TYPES_HPP

#include <chrono>

// Common type aliases
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

// Enums
enum class Side { BUY, SELL };

enum class Type { NEW, CANCEL, AMEND };

#endif // TYPES_HPP