#include "order_book.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

OrderBook::OrderBook() {}

bool OrderBook::cancel_order(int order_id) {
  Timer timer;
  timer.start();

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

void OrderBook::print_order_status(int order_id) const {
  auto order = get_order(order_id);
  if (order) {
    std::cout << "\n=== Order Status ===" << std::endl;
    std::cout << *order << std::endl;
  } else {
    std::cout << "Order " << order_id << " not found." << std::endl;
  }
}

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
  // FOK: Check if ENTIRE order can be filled before matching
  if (buy_order.tif == TimeInForce::FOK) {
    if (!can_fill_order(buy_order)) {
      // Cancel entire order
      auto it = active_orders_.find(buy_order.id);
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
      }
      std::cout << " FOK order " << buy_order.id
                << " cancelled (insufficient liquidity to fill "
                << buy_order.quantity << " shares)" << std::endl;
      return; // Don't match anything
    }
  }

  while (buy_order.remaining_qty > 0 && !asks_.empty()) {
    Order best_ask = asks_.top();

    if (buy_order.is_market_order() || buy_order.price >= best_ask.price) {
      int trade_qty = std::min(buy_order.remaining_qty, best_ask.remaining_qty);
      double trade_price = best_ask.price;

      fills_.emplace_back(buy_order.id, best_ask.id, trade_price, trade_qty);

      buy_order.remaining_qty -= trade_qty;
      best_ask.remaining_qty -= trade_qty;

      asks_.pop();

      if (best_ask.remaining_qty > 0) {
        asks_.push(best_ask);
      }

      // Update both orders in active_orders_
      auto buy_it = active_orders_.find(buy_order.id);
      if (buy_it != active_orders_.end()) {
        buy_it->second.remaining_qty = buy_order.remaining_qty;
        if (buy_order.is_filled()) {
          buy_it->second.state = OrderState::FILLED;
        } else if (buy_order.remaining_qty < buy_order.quantity) {
          buy_it->second.state = OrderState::PARTIALLY_FILLED;
        }
      }

      auto ask_it = active_orders_.find(best_ask.id);
      if (ask_it != active_orders_.end()) {
        ask_it->second.remaining_qty = best_ask.remaining_qty;
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
      // GTC or DAY: Add to book
      bids_.push(buy_order);
    } else {
      // IOC or FOK: Cancel remainder
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
  // FOK: Check if ENTIRE order can be filled before matching
  if (sell_order.tif == TimeInForce::FOK) {
    if (!can_fill_order(sell_order)) {
      // Cancel entire order
      auto it = active_orders_.find(sell_order.id);
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
      }
      std::cout << "FOK order " << sell_order.id
                << " cancelled (insufficient liquidity to fill "
                << sell_order.quantity << " shares)" << std::endl;
      return; // Don't match anything
    }
  }

  while (sell_order.remaining_qty > 0 && !bids_.empty()) {
    Order best_bid = bids_.top();

    if (sell_order.is_market_order() || sell_order.price <= best_bid.price) {
      int trade_qty =
          std::min(sell_order.remaining_qty, best_bid.remaining_qty);
      double trade_price = best_bid.price;

      fills_.emplace_back(best_bid.id, sell_order.id, trade_price, trade_qty);

      sell_order.remaining_qty -= trade_qty;
      best_bid.remaining_qty -= trade_qty;

      bids_.pop();

      if (best_bid.remaining_qty > 0) {
        bids_.push(best_bid);
      }

      // Update both orders in active_orders_
      auto sell_it = active_orders_.find(sell_order.id);
      if (sell_it != active_orders_.end()) {
        sell_it->second.remaining_qty = sell_order.remaining_qty;
        if (sell_order.is_filled()) {
          sell_it->second.state = OrderState::FILLED;
        } else if (sell_order.remaining_qty < sell_order.quantity) {
          sell_it->second.state = OrderState::PARTIALLY_FILLED;
        }
      }

      auto bid_it = active_orders_.find(best_bid.id);
      if (bid_it != active_orders_.end()) {
        bid_it->second.remaining_qty = best_bid.remaining_qty;
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
      // GTC or DAY: Add to book
      asks_.push(sell_order);
    } else {
      // IOC or FOK: Cancel remainder
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

void OrderBook::add_order(Order o) {
  Timer timer;
  timer.start();

  Order order = o;
  order.state = OrderState::ACTIVE; // Mark as active

  // Track the order
  active_orders_.insert({order.id, order});

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