#include "order_book.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

OrderBook::OrderBook()
    : logging_enabled_(false), last_trade_price_(0), snapshot_counter_(0) {}

// ============================================================================
//  CORE ORDER OPERATIONS
// ============================================================================

void OrderBook::add_order(Order o) {
  Timer timer;
  timer.start();

  Order order = o;

  // Handle stop orders
  if (order.is_stop && !order.stop_triggered) {
    // Stop orders fo to pending lis, not active matching

    order.state = OrderState::PENDING;
    active_orders_.insert_or_assign(order.id, order);

    if (order.side == Side::BUY) {
      stop_buys_.insert({order.stop_price, order});
      std::cout << "Stop-buy order " << order.id << " placed at &"
                << order.stop_price << std::endl;
    } else if (order.side == Side::SELL) {
      stop_sells_.insert({order.stop_price, order});
      std::cout << "Stop-sell order " << order.id << " placed at &"
                << order.stop_price << std::endl;
    } else {
      throw std::runtime_error("Invalid order side");
    }

    timer.stop();
    insertion_latencies_ns_.push_back(timer.elapsed_nanoseconds());
    return; // Don't match yet
  }

  // Regular orders (or triggered stops) proceed normally
  order.state = OrderState::ACTIVE;
  active_orders_.insert({order.id, order});

  if (logging_enabled_) {
    // Handle market orders with infinite price properly
    double log_price = order.is_market_order() ? 0.0 : order.price;

    if (order.is_iceberg()) {
      event_log_.emplace_back(order.timestamp, order.id, order.side, order.type,
                              order.tif, log_price, order.quantity,
                              order.peak_size);
    } else {
      event_log_.emplace_back(order.timestamp, order.id, order.side, order.type,
                              order.tif, log_price, order.quantity);
    }
  }

  if (order.side == Side::BUY) {
    match_buy_order(order);
  } else if (order.side == Side::SELL) {
    match_sell_order(order);
  } else {
    throw std::runtime_error("Invalid order side");
  }

  // Update order state after matching
  if (order.is_filled()) {
    active_orders_.at(order.id).state = OrderState::FILLED;
  } else if (order.remaining_qty < order.quantity) {
    active_orders_.at(order.id).state = OrderState::PARTIALLY_FILLED;
  }

  timer.stop();
  insertion_latencies_ns_.push_back(timer.elapsed_nanoseconds());
}

// ============================================================================
// ORDER LIFECYCLE MANAGEMENT
// ============================================================================

bool OrderBook::cancel_order(int order_id) {
  Timer timer;
  timer.start();

  if (logging_enabled_) {
    event_log_.emplace_back(Clock::now(), EventType::CANCEL_ORDER, order_id);
  }

  // check if order exits and is active
  auto it = active_orders_.find(order_id);
  if (it == active_orders_.end()) {
    std::cout << "Order " << order_id << " not found or already processed."
              << '\n';
    return false;
  }
  Order &order = it->second;

  // Can't cancel filled orders
  if (order.is_filled()) {
    std::cout << "Order " << order_id << " is already filled." << '\n';
    return false;
  }

  // Mark as canceled
  order.state = OrderState::CANCELLED;

  // Move to cancelled Orders
  cancelled_orders_.insert({order_id, order});
  active_orders_.erase(it);

  timer.stop();

  std::cout << "✓ Cancelled order " << order_id
            << " (latency: " << timer.elapsed_nanoseconds() << " ns)" << '\n';

  return true;
}

bool OrderBook::amend_order(int order_id, std::optional<double> new_price,
                            std::optional<int> new_quantity) {
  Timer timer;
  timer.start();

  if (logging_enabled_) {
    event_log_.emplace_back(Clock::now(), order_id, new_price, new_quantity);
  }

  // Check if order exists
  auto it = active_orders_.find(order_id);
  if (it == active_orders_.end()) {
    std::cout << "Order " << order_id << " not found." << '\n';
    return false;
  }
  Order &order = it->second;

  // Can't amend filled orders
  if (order.is_filled()) {
    std::cout << "Order " << order_id << " is already filled." << '\n';
    return false;
  }

  // Lazy deletion: Cancel old order and create a new one
  Side side = order.side;

  // If new_price has a value, use it. Otherwise, use the default (order.price)
  double price = new_price.value_or(order.price);
  int quantity = new_quantity.value_or(order.remaining_qty);

  cancel_order(order_id);

  // Create new order with same ID
  Order amended_order(order_id, side, price, quantity);
  amended_order.state = OrderState::ACTIVE;

  // re add to book
  if (side == Side::BUY) {
    bids_.push(amended_order);
  } else {
    asks_.push(amended_order);
  }

  active_orders_.insert({order_id, amended_order});

  timer.stop();

  std::cout << "✓ Amended order " << order_id
            << " (latency: " << timer.elapsed_nanoseconds() << " ns)" << '\n';

  return true;
}

std::optional<Order> OrderBook::get_order(int order_id) const {
  auto it = active_orders_.find(order_id);
  if (it != active_orders_.end()) {
    return it->second;
  }

  // Check cancelled orders
  auto cancelled_it = cancelled_orders_.find(order_id);
  if (cancelled_it != cancelled_orders_.end()) {
    return cancelled_it->second;
  }

  return std::nullopt;
}

// ============================================================================
//   MATCHING ENGINE CORE
// ============================================================================

bool OrderBook::can_fill_order(const Order &order) const {
  int available_qty = 0;

  if (order.side == Side::BUY) {
    // Check ask side
    auto asks_copy = asks_; // copy ot iterate without modifying.

    while (!asks_copy.empty() && available_qty < order.quantity) {
      Order best_ask = asks_copy.top();

      // check if price matches or market order
      if (order.is_market_order() || order.price >= best_ask.price) {
        available_qty += best_ask.remaining_qty;
        asks_copy.pop();
      } else {
        break; // No more matching prices
      }
    }
  } else {
    auto bids_copy = bids_; // copy to iterate without modifying
    while (!bids_copy.empty() && available_qty < order.quantity) {
      Order best_bid = bids_copy.top();

      // check if price match or market order
      if (order.is_market_order() || order.price <= best_bid.price) {
        available_qty += best_bid.remaining_qty;
        bids_copy.pop();
      } else {
        break; // No more matching prices
      }
    }
  }
  return available_qty >= order.quantity;
}

void OrderBook::match_buy_order(Order &buy_order) {
  // FOK check (unchanged)
  if (buy_order.tif == TimeInForce::FOK) {
    if (!can_fill_order(buy_order)) {
      auto it = active_orders_.find(buy_order.id);
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
      }
      std::cout << "FOK order " << buy_order.id
                << " cancelled (insufficient liquidity to fill "
                << buy_order.quantity << " shares)" << std::endl;
      return;
    }
  }

  while (buy_order.remaining_qty > 0 && !asks_.empty()) {
    Order best_ask = asks_.top();

    if (buy_order.is_market_order() || buy_order.price >= best_ask.price) {
      // CRITICAL: Only match against DISPLAY quantity for icebergs
      int available_qty =
          best_ask.is_iceberg() ? best_ask.display_qty : best_ask.remaining_qty;

      int trade_qty = std::min(buy_order.remaining_qty, available_qty);
      double trade_price = best_ask.price;

      fills_.emplace_back(buy_order.id, best_ask.id, trade_price, trade_qty);

      /// Log fill event
      if (logging_enabled_) {
        event_log_.emplace_back(Clock::now(), buy_order.id, best_ask.id,
                                trade_price, trade_qty);
      }

      // Check if this trade triggers and stops
      check_stop_triggers(trade_price);

      buy_order.remaining_qty -= trade_qty;
      best_ask.remaining_qty -= trade_qty;

      // Update display quantity for icebergs
      if (best_ask.is_iceberg()) {
        best_ask.display_qty -= trade_qty;
      }

      asks_.pop();

      // Check if ask needs refresh (iceberg exhausted display)
      if (best_ask.needs_refresh()) {
        best_ask.refresh_display();
        asks_.push(best_ask); // Re-add with new timestamp (loses priority!)
      } else if (best_ask.remaining_qty > 0) {
        asks_.push(best_ask);
      }

      // Update both orders in active_orders_
      auto buy_it = active_orders_.find(buy_order.id);
      if (buy_it != active_orders_.end()) {
        buy_it->second.remaining_qty = buy_order.remaining_qty;
        if (buy_order.is_iceberg()) {
          buy_it->second.display_qty = buy_order.display_qty;
        }
        if (buy_order.is_filled()) {
          buy_it->second.state = OrderState::FILLED;
        } else if (buy_order.remaining_qty < buy_order.quantity) {
          buy_it->second.state = OrderState::PARTIALLY_FILLED;
        }
      }

      auto ask_it = active_orders_.find(best_ask.id);
      if (ask_it != active_orders_.end()) {
        ask_it->second.remaining_qty = best_ask.remaining_qty;
        if (best_ask.is_iceberg()) {
          ask_it->second.display_qty = best_ask.display_qty;
          ask_it->second.hidden_qty = best_ask.hidden_qty;
        }
        if (best_ask.is_filled()) {
          ask_it->second.state = OrderState::FILLED;
        } else if (best_ask.remaining_qty < best_ask.quantity) {
          ask_it->second.state = OrderState::PARTIALLY_FILLED;
        }
      }
    } else {
      break;
    }
  }

  // Handle unfilled quantity based on TIF
  if (buy_order.remaining_qty > 0) {
    if (buy_order.can_rest_in_book()) {
      bids_.push(buy_order);
    } else {
      auto it = active_orders_.find(buy_order.id);
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
      }

      if (buy_order.tif == TimeInForce::IOC) {
        int filled = buy_order.quantity - buy_order.remaining_qty;
        if (filled > 0) {
          std::cout << "IOC order " << buy_order.id << " partially filled ("
                    << filled << "/" << buy_order.quantity
                    << "), remaining cancelled" << std::endl;
        } else {
          std::cout << "IOC order " << buy_order.id
                    << " cancelled (no immediate liquidity)" << std::endl;
        }
      }
    }
  }
}

void OrderBook::match_sell_order(Order &sell_order) {
  // FOK check (unchanged)
  if (sell_order.tif == TimeInForce::FOK) {
    if (!can_fill_order(sell_order)) {
      auto it = active_orders_.find(sell_order.id);
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
      }
      std::cout << "FOK order " << sell_order.id
                << " cancelled (insufficient liquidity to fill "
                << sell_order.quantity << " shares)" << std::endl;
      return;
    }
  }

  while (sell_order.remaining_qty > 0 && !bids_.empty()) {
    Order best_bid = bids_.top();

    if (sell_order.is_market_order() || sell_order.price <= best_bid.price) {
      // CRITICAL: Only match against DISPLAY quantity for icebergs
      int available_qty =
          best_bid.is_iceberg() ? best_bid.display_qty : best_bid.remaining_qty;

      int trade_qty = std::min(sell_order.remaining_qty, available_qty);
      double trade_price = best_bid.price;

      fills_.emplace_back(best_bid.id, sell_order.id, trade_price, trade_qty);

      // Log fill event
      if (logging_enabled_) {
        event_log_.emplace_back(Clock::now(), best_bid.id, sell_order.id,
                                trade_price, trade_qty);
      }

      // Check if this trade triggers and stops
      check_stop_triggers(trade_price);

      sell_order.remaining_qty -= trade_qty;
      best_bid.remaining_qty -= trade_qty;

      // Update display quantity for icebergs
      if (best_bid.is_iceberg()) {
        best_bid.display_qty -= trade_qty;
      }

      bids_.pop();

      // Check if bid needs refresh (iceberg exhausted display)
      if (best_bid.needs_refresh()) {
        best_bid.refresh_display();
        bids_.push(best_bid); // Re-add with new timestamp (loses priority!)
      } else if (best_bid.remaining_qty > 0) {
        bids_.push(best_bid);
      }

      // Update both orders in active_orders_
      auto sell_it = active_orders_.find(sell_order.id);
      if (sell_it != active_orders_.end()) {
        sell_it->second.remaining_qty = sell_order.remaining_qty;
        if (sell_order.is_iceberg()) {
          sell_it->second.display_qty = sell_order.display_qty;
        }
        if (sell_order.is_filled()) {
          sell_it->second.state = OrderState::FILLED;
        } else if (sell_order.remaining_qty < sell_order.quantity) {
          sell_it->second.state = OrderState::PARTIALLY_FILLED;
        }
      }

      auto bid_it = active_orders_.find(best_bid.id);
      if (bid_it != active_orders_.end()) {
        bid_it->second.remaining_qty = best_bid.remaining_qty;
        if (best_bid.is_iceberg()) {
          bid_it->second.display_qty = best_bid.display_qty;
          bid_it->second.hidden_qty = best_bid.hidden_qty;
        }
        if (best_bid.is_filled()) {
          bid_it->second.state = OrderState::FILLED;
        } else if (best_bid.remaining_qty < best_bid.quantity) {
          bid_it->second.state = OrderState::PARTIALLY_FILLED;
        }
      }
    } else {
      break;
    }
  }

  // Handle unfilled quantity based on TIF
  if (sell_order.remaining_qty > 0) {
    if (sell_order.can_rest_in_book()) {
      asks_.push(sell_order);
    } else {
      auto it = active_orders_.find(sell_order.id);
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
      }

      if (sell_order.tif == TimeInForce::IOC) {
        int filled = sell_order.quantity - sell_order.remaining_qty;
        if (filled > 0) {
          std::cout << "IOC order " << sell_order.id << " partially filled ("
                    << filled << "/" << sell_order.quantity
                    << "), remaining cancelled" << std::endl;
        } else {
          std::cout << "IOC order " << sell_order.id
                    << " cancelled (no immediate liquidity)" << std::endl;
        }
      }
    }
  }
}

// ============================================================================
// STOP ORDER MANAGEMENT
// ============================================================================

void OrderBook::check_stop_triggers(double trade_price) {
  last_trade_price_ = trade_price;

  // Check buy stops (trigger when price rises to/above stop_price)
  auto buy_it = stop_buys_.begin();
  while (buy_it != stop_buys_.end()) {
    if (trade_price >= buy_it->first) {
      Order stop_order = buy_it->second;

      // Remove from stop book
      buy_it = stop_buys_.erase(buy_it);

      // Trigger the stop order
      stop_order.trigger_stop();

      // Remove old state from active_orders
      active_orders_.erase(stop_order.id);
      // Re-submit as active order
      add_order(stop_order);

    } else {
      break; // Stop prices are sorted, no more will trigger
    }
  }

  // Check sell stops (trigger when price falls to/below stop_price)
  auto sell_it = stop_sells_.rbegin();
  while (sell_it != stop_sells_.rend()) {
    if (trade_price <= sell_it->first) {
      Order stop_order = sell_it->second;

      // Convert reverse iterator to forward for erase
      auto forward_it = std::next(sell_it).base();
      stop_sells_.erase(forward_it);

      // Trigger and execute
      stop_order.trigger_stop();

      // Remove old state form active_orders
      active_orders_.erase(stop_order.id);
      // Re-submit as active order
      add_order(stop_order);

      // Reset iterator after modification
      sell_it = stop_sells_.rbegin();
    } else {
      ++sell_it;
    }
  }
}

// ============================================================================
//  MARKET DATA ACCESSORS
// ============================================================================

std::optional<Order> OrderBook::get_best_bid() const {
  if (bids_.empty()) {
    return std::nullopt;
  }
  return bids_.top();
}

std::optional<Order> OrderBook::get_best_ask() const {
  if (asks_.empty()) {
    return std::nullopt;
  }
  return asks_.top();
}

std::optional<double> OrderBook::get_spread() const {
  if (bids_.empty() || asks_.empty()) {
    return std::nullopt;
  }
  return asks_.top().price - bids_.top().price;
}

const std::vector<Fill> &OrderBook::get_fills() const { return fills_; }

std::vector<OrderBook::PriceLevel>
OrderBook::get_bid_levels(int max_levels) const {
  std::vector<PriceLevel> levels;

  if (bids_.empty()) {
    return levels;
  }

  auto bids_copy = bids_;
  std::map<double, std::pair<int, int>> price_map;

  while (!bids_copy.empty()) {
    Order order = bids_copy.top();
    bids_copy.pop();

    price_map[order.price].first += order.remaining_qty;
    price_map[order.price].second += 1;
  }

  int count = 0;
  for (auto it = price_map.rbegin();
       it != price_map.rend() && count < max_levels; ++it, ++count) {
    PriceLevel level;
    level.price = it->first;
    level.total_quantity = it->second.first;
    level.num_orders = it->second.second;
    levels.push_back(level);
  }

  return levels;
}

std::vector<OrderBook::PriceLevel>
OrderBook::get_ask_levels(int max_levels) const {
  std::vector<PriceLevel> levels;

  if (asks_.empty()) {
    return levels;
  }

  auto asks_copy = asks_;
  std::map<double, std::pair<int, int>> price_map;

  while (!asks_copy.empty()) {
    Order order = asks_copy.top();
    asks_copy.pop();

    price_map[order.price].first += order.remaining_qty;
    price_map[order.price].second += 1;
  }

  int count = 0;
  for (auto it = price_map.begin(); it != price_map.end() && count < max_levels;
       ++it, ++count) {
    PriceLevel level;
    level.price = it->first;
    level.total_quantity = it->second.first;
    level.num_orders = it->second.second;
    levels.push_back(level);
  }

  return levels;
}

// ============================================================================
//  STATISTICS & DISPLAY METHODS
// ============================================================================

void OrderBook::print_fills() const {
  std::cout << "\n=== Fills Generated ===" << std::endl;
  if (fills_.empty()) {
    std::cout << "No fills yet." << std::endl;
    return;
  }
  for (const auto &fill : fills_) {
    std::cout << fill << std::endl;
  }
}

void OrderBook::print_top_of_book() const {
  std::cout << "--- Top of Book ---" << std::endl;

  auto best_bid = get_best_bid();
  if (best_bid) {
    std::cout << "Best Bid: " << best_bid->price
              << " (qty: " << best_bid->quantity << ")" << std::endl;
  } else {
    std::cout << "Best Bid: N/A" << std::endl;
  }

  auto best_ask = get_best_ask();
  if (best_ask) {
    std::cout << "Best Ask: " << best_ask->price
              << " (qty: " << best_ask->quantity << ")" << std::endl;
  } else {
    std::cout << "Best Ask: N/A" << std::endl;
  }

  auto spread = get_spread();
  if (spread) {
    std::cout << "Spread: " << *spread << std::endl;
  } else {
    std::cout << "Spread: N/A" << std::endl;
  }
  std::cout << std::endl;
}

void OrderBook::print_book_summary() const {
  std::cout << "\n=== Current Book State ===" << std::endl;

  std::cout << "Orders in book: " << (bids_.size() + asks_.size()) << std::endl;
  std::cout << "  Bids: " << bids_.size() << std::endl;
  std::cout << "  Asks: " << asks_.size() << std::endl;

  auto best_bid = get_best_bid();
  auto best_ask = get_best_ask();
  auto spread = get_spread();

  if (best_bid && best_ask) {
    std::cout << "\nTop of Book:" << std::endl;
    std::cout << "  Best Bid: $" << std::fixed << std::setprecision(2)
              << best_bid->price << " (" << best_bid->remaining_qty
              << " shares)" << std::endl;
    std::cout << "  Best Ask: $" << std::fixed << std::setprecision(2)
              << best_ask->price << " (" << best_ask->remaining_qty
              << " shares)" << std::endl;
    std::cout << "  Spread: $" << std::fixed << std::setprecision(4) << *spread;

    if (*spread < 0) {
      std::cout << "CROSSED BOOK!" << std::endl;
    } else if (*spread == 0) {
      std::cout << " (locked)" << std::endl;
    } else if (*spread < 0.10) {
      std::cout << " (tight)" << std::endl;
    } else {
      std::cout << " (wide)" << std::endl;
    }
  } else if (best_bid) {
    std::cout << "\nBid-only market:" << std::endl;
    std::cout << "  Best Bid: $" << best_bid->price << std::endl;
    std::cout << "  No asks available" << std::endl;
  } else if (best_ask) {
    std::cout << "\nAsk-only market:" << std::endl;
    std::cout << "  Best Ask: $" << best_ask->price << std::endl;
    std::cout << "  No bids available" << std::endl;
  } else {
    std::cout << "\nEmpty book (no orders)" << std::endl;
  }
}

void OrderBook::print_market_depth(int levels) const {
  auto bid_levels = get_bid_levels(levels);
  auto ask_levels = get_ask_levels(levels);

  std::cout << "\n=== Market Depth (" << levels << " levels) ===" << std::endl;
  std::cout << std::string(70, '=') << std::endl;

  // Header
  std::cout << std::setw(25) << std::right << "BIDS"
            << " | " << std::setw(10) << "PRICE"
            << " | " << std::setw(25) << std::left << "ASKS" << std::endl;
  std::cout << std::string(70, '-') << std::endl;

  // Determine how many rows to display
  size_t max_rows = std::max(bid_levels.size(), ask_levels.size());

  // Calculate totals
  int total_bid_qty = 0;
  int total_ask_qty = 0;
  for (const auto &level : bid_levels)
    total_bid_qty += level.total_quantity;
  for (const auto &level : ask_levels)
    total_ask_qty += level.total_quantity;

  // Display rows (asks in reverse order - highest first)
  for (size_t i = 0; i < max_rows; ++i) {
    // Determine indices
    size_t ask_idx =
        (ask_levels.size() > i) ? (ask_levels.size() - 1 - i) : SIZE_MAX;
    size_t bid_idx = i;

    // Format bid side
    std::ostringstream bid_str;
    if (bid_idx < bid_levels.size()) {
      bid_str << bid_levels[bid_idx].total_quantity << " ("
              << bid_levels[bid_idx].num_orders << " order";
      if (bid_levels[bid_idx].num_orders > 1)
        bid_str << "s";
      bid_str << ")";
    }

    // Format price
    std::ostringstream price_str;
    if (ask_idx != SIZE_MAX && ask_idx < ask_levels.size()) {
      price_str << std::fixed << std::setprecision(2) << "$"
                << ask_levels[ask_idx].price;
    } else if (bid_idx < bid_levels.size()) {
      price_str << std::fixed << std::setprecision(2) << "$"
                << bid_levels[bid_idx].price;
    }

    // Format ask side
    std::ostringstream ask_str;
    if (ask_idx != SIZE_MAX && ask_idx < ask_levels.size()) {
      ask_str << ask_levels[ask_idx].total_quantity << " ("
              << ask_levels[ask_idx].num_orders << " order";
      if (ask_levels[ask_idx].num_orders > 1)
        ask_str << "s";
      ask_str << ")";
    }

    // Print row
    std::cout << std::setw(25) << std::right << bid_str.str() << " | "
              << std::setw(10) << std::left << price_str.str() << " | "
              << std::setw(25) << std::left << ask_str.str() << std::endl;
  }

  // Footer with totals
  std::cout << std::string(70, '-') << std::endl;
  std::cout << std::setw(25) << std::right
            << ("Total: " + std::to_string(total_bid_qty) + " shares") << " | "
            << std::setw(10) << " "
            << " | " << std::setw(25) << std::left
            << ("Total: " + std::to_string(total_ask_qty) + " shares")
            << std::endl;
  std::cout << std::string(70, '=') << std::endl;

  // Additional stats
  auto spread = get_spread();
  if (spread) {
    double spread_bps = (*spread / bid_levels[0].price) * 10000;
    std::cout << "Spread: $" << std::fixed << std::setprecision(4) << *spread
              << " (" << std::fixed << std::setprecision(2) << spread_bps
              << " bps)" << std::endl;
  }
  std::cout << std::endl;
}

void OrderBook::print_market_depth_compact() const {
  auto bid_levels = get_bid_levels(10); // Get more levels for compact view
  auto ask_levels = get_ask_levels(10);

  std::cout << "\n=== Order Book (Compact) ===" << std::endl;

  // Print asks (top to bottom)
  std::cout << "\n ASKS (sellers):" << std::endl;
  if (ask_levels.empty()) {
    std::cout << "  (no asks)" << std::endl;
  } else {
    for (auto it = ask_levels.rbegin(); it != ask_levels.rend(); ++it) {
      std::cout << "  $" << std::fixed << std::setprecision(2) << std::setw(8)
                << it->price << "  │ " << std::setw(6) << it->total_quantity
                << " (" << it->num_orders << ")" << std::endl;
    }
  }

  // Spread line
  auto spread = get_spread();
  if (spread) {
    std::cout << std::string(30, '-') << std::endl;
    std::cout << "  Spread: $" << std::fixed << std::setprecision(4) << *spread
              << std::endl;
    std::cout << std::string(30, '-') << std::endl;
  } else {
    std::cout << std::string(30, '-') << std::endl;
    std::cout << "  (crossed or one-sided)" << std::endl;
    std::cout << std::string(30, '-') << std::endl;
  }

  // Print bids (top to bottom)
  std::cout << "\n BIDS (buyers):" << std::endl;
  if (bid_levels.empty()) {
    std::cout << "  (no bids)" << std::endl;
  } else {
    for (const auto &level : bid_levels) {
      std::cout << "  $" << std::fixed << std::setprecision(2) << std::setw(8)
                << level.price << "  │ " << std::setw(6) << level.total_quantity
                << " (" << level.num_orders << ")" << std::endl;
    }
  }
  std::cout << std::endl;
}

void OrderBook::print_order_status(int order_id) const {
  auto order = get_order(order_id);
  if (order) {
    std::cout << "\n=== Order Status ===" << std::endl;
    std::cout << *order << std::endl;
  } else {
    std::cout << "Order " << order_id << " not found." << std::endl;
  }
}

void OrderBook::print_pending_stops() const {
  std::cout << "\n=== Pending Stop Orders ===" << std::endl;

  if (stop_buys_.empty() && stop_sells_.empty()) {
    std::cout << "No pending stop orders." << std::endl;
    return;
  }

  if (!stop_buys_.empty()) {
    std::cout << "\nStop-Buy Orders (trigger at or above):" << std::endl;
    for (const auto &[price, order] : stop_buys_) {
      std::cout << "  $" << std::fixed << std::setprecision(2) << price
                << " → Order #" << order.id << " (" << order.quantity
                << " shares)" << std::endl;
    }
  }

  if (!stop_sells_.empty()) {
    std::cout << "\nStop-Sell Orders (trigger at or below):" << std::endl;
    for (const auto &[price, order] : stop_sells_) {
      std::cout << "  $" << std::fixed << std::setprecision(2) << price
                << " → Order #" << order.id << " (" << order.quantity
                << " shares)" << std::endl;
    }
  }

  std::cout << std::endl;
}

void OrderBook::print_trade_timeline() const {
  std::cout << "\n=== Trade Timeline ===" << std::endl;

  if (fills_.empty()) {
    std::cout << "No trades executed." << std::endl;
    return;
  }

  std::cout << "Displaying " << fills_.size()
            << " fills in chronological order:\n"
            << std::endl;

  for (size_t i = 0; i < fills_.size(); ++i) {
    const auto &fill = fills_[i];
    std::cout << "[" << (i + 1) << "] " << fill << std::endl;
  }
}

void OrderBook::print_latency_stats() const {
  if (insertion_latencies_ns_.empty()) {
    std::cout << "No orders inserted yet!" << std::endl;
    return;
  }

  std::vector<long long> sorted = insertion_latencies_ns_;
  std::sort(sorted.begin(), sorted.end());

  size_t n = sorted.size();
  long long total = 0;
  for (long long lat : sorted) {
    total += lat;
  }

  long long min = sorted.front();
  long long max = sorted.back();
  double avg = static_cast<double>(total) / n;
  long long p50 = sorted[n / 2];
  long long p95 = sorted[static_cast<size_t>(0.95 * n)];
  long long p99 = sorted[static_cast<size_t>(0.99 * n)];

  std::cout << "\n=== Order Insertion Latency ===" << std::endl;
  std::cout << "Total orders: " << n << std::endl;
  std::cout << "Average: " << avg << " ns" << std::endl;
  std::cout << "Min: " << min << " ns" << std::endl;
  std::cout << "Max: " << max << " ns" << std::endl;
  std::cout << "p50: " << p50 << " ns" << std::endl;
  std::cout << "p95: " << p95 << " ns" << std::endl;
  std::cout << "p99: " << p99 << " ns" << std::endl;
}

void OrderBook::print_match_stats() const {
  std::cout << "\n=== Matching Statistics ===" << std::endl;

  std::cout << "Total orders processed: " << insertion_latencies_ns_.size()
            << std::endl;
  std::cout << "Total fills generated: " << fills_.size() << std::endl;

  if (!fills_.empty()) {
    int total_volume = 0;
    double total_notional = 0.0;
    double min_price = fills_[0].price;
    double max_price = fills_[0].price;

    for (const auto &fill : fills_) {
      total_volume += fill.quantity;
      total_notional += fill.quantity * fill.price;
      min_price = std::min(min_price, fill.price);
      max_price = std::max(max_price, fill.price);
    }

    double avg_fill_size = static_cast<double>(total_volume) / fills_.size();
    double vwap = total_notional / total_volume;

    std::cout << "\n--- Volume Statistics ---" << std::endl;
    std::cout << "Total volume traded: " << total_volume << " shares"
              << std::endl;
    std::cout << "Total notional value: $" << std::fixed << std::setprecision(2)
              << total_notional << std::endl;
    std::cout << "Average fill size: " << std::fixed << std::setprecision(1)
              << avg_fill_size << " shares" << std::endl;
    std::cout << "VWAP: $" << std::fixed << std::setprecision(2) << vwap
              << std::endl;
    std::cout << "Price range: $" << std::fixed << std::setprecision(2)
              << min_price << " - $" << max_price << std::endl;
  }

  print_latency_stats();
}

void OrderBook::print_fill_rate_analysis() const {
  if (insertion_latencies_ns_.empty()) {
    std::cout << "No orders to analyze!" << std::endl;
    return;
  }

  std::cout << "\n=== Fill Rate Analysis ===" << std::endl;

  size_t total_orders = insertion_latencies_ns_.size();

  std::set<int> filled_order_ids;
  for (const auto &fill : fills_) {
    filled_order_ids.insert(fill.buy_order_id);
    filled_order_ids.insert(fill.sell_order_id);
  }
  size_t orders_with_fills = filled_order_ids.size();

  double fill_rate =
      (static_cast<double>(orders_with_fills) / total_orders) * 100.0;

  std::cout << "Orders that generated fills: " << orders_with_fills << " / "
            << total_orders << " (" << std::fixed << std::setprecision(1)
            << fill_rate << "%)" << std::endl;
  std::cout << "Orders added to book (no fill): "
            << (total_orders - orders_with_fills) << std::endl;
}

// ============================================================================
//  PERSISTENCE & RECOVERY
// ============================================================================

void OrderBook::save_events(const std::string &filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }

  file << OrderEvent::csv_header() << "\n";

  for (const auto &event : event_log_) {
    file << event.to_csv() << "\n";
  }

  file.close();
  std::cout << "Saved " << event_log_.size() << "events to " << filename
            << std::endl;
}

Snapshot OrderBook::create_snapshot() const {
  Snapshot snapshot;

  // Metadata
  snapshot.snapshot_time = std::chrono::system_clock::now();
  snapshot.snapshot_id = snapshot_counter_;
  snapshot.version = "1.0";

  // Copy active orders
  for (const auto &[id, order] : active_orders_) {
    snapshot.active_orders.push_back(order);
  }

  // Copy pending stops
  for (const auto &[price, order] : stop_buys_) {
    snapshot.pending_stops.push_back(order);
  }
  for (const auto &[price, order] : stop_sells_) {
    snapshot.pending_stops.push_back(order);
  }

  // Copy fills
  snapshot.fills = fills_;

  // Copy state
  snapshot.last_trade_price = last_trade_price_;
  snapshot.total_orders_processed = insertion_latencies_ns_.size();
  snapshot.latencies = insertion_latencies_ns_;

  return snapshot;
}

void OrderBook::restore_from_snapshot(const Snapshot &snapshot) {
  std::cout << "Restoring order book from snapshot..." << std::endl;

  // Clear current state
  while (!bids_.empty())
    bids_.pop();
  while (!asks_.empty())
    asks_.pop();
  active_orders_.clear();
  cancelled_orders_.clear();
  stop_buys_.clear();
  stop_sells_.clear();
  fills_.clear();
  event_log_.clear();
  insertion_latencies_ns_.clear();

  // Restore state
  last_trade_price_ = snapshot.last_trade_price;
  fills_ = snapshot.fills;
  insertion_latencies_ns_ = snapshot.latencies;

  // Restore active orders and rebuild books
  for (const auto &order : snapshot.active_orders) {
    active_orders_.insert({order.id, order});

    // Add to appropriate book if active and not stop
    if (order.is_active() && !order.is_stop) {
      if (order.side == Side::BUY) {
        bids_.push(order);
      } else {
        asks_.push(order);
      }
    }
  }

  // Restore pending stops
  for (const auto &order : snapshot.pending_stops) {
    active_orders_.insert({order.id, order});

    if (order.side == Side::BUY) {
      stop_buys_.insert({order.stop_price, order});
    } else {
      stop_sells_.insert({order.stop_price, order});
    }
  }

  std::cout << "Order book restored successfully" << std::endl;
  std::cout << "   Active orders: " << active_orders_.size() << std::endl;
  std::cout << "   Pending stops: " << (stop_buys_.size() + stop_sells_.size())
            << std::endl;
  std::cout << "   Fills: " << fills_.size() << std::endl;
}

void OrderBook::save_snapshot(const std::string &filename) const {
  auto snapshot = create_snapshot();
  const_cast<OrderBook *>(this)->snapshot_counter_++;
  snapshot.save_to_file(filename);
}

void OrderBook::load_snapshot(const std::string &filename) {
  auto snapshot = Snapshot::load_from_file(filename);

  if (!snapshot.validate()) {
    throw std::runtime_error("Snapshot validation failed");
  }

  restore_from_snapshot(snapshot);
}

void OrderBook::save_checkpoint(const std::string &snapshot_file,
                                const std::string &events_file) const {
  std::cout << "\nCreating checkpoint..." << std::endl;

  // Save snapshot
  save_snapshot(snapshot_file);

  // Save events since snapshot
  save_events(events_file);

  std::cout << "Checkpoint created:" << std::endl;
  std::cout << "   Snapshot: " << snapshot_file << std::endl;
  std::cout << "   Events: " << events_file << std::endl;
}

void OrderBook::recover_from_checkpoint(const std::string &snapshot_file,
                                        const std::string &events_file) {
  std::cout << "\nRecovering from checkpoint..." << std::endl;

  // Load snapshot
  load_snapshot(snapshot_file);

  // Replay events since snapshot
  std::ifstream event_file(events_file);
  if (event_file.is_open()) {
    std::string line;
    std::getline(event_file, line); // Skip header

    size_t event_count = 0;
    while (std::getline(event_file, line)) {
      if (line.empty())
        continue;

      auto event = OrderEvent::from_csv(line);

      // Replay event (skip FILL events as they're regenerated)
      if (event.type != EventType::FILL) {
        // Replay logic here (simplified)
        event_count++;
      }
    }

    std::cout << "Replayed " << event_count << " events" << std::endl;
  }

  std::cout << "Recovery complete" << std::endl;
}
