#include "order_book.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

OrderBook::OrderBook() {}

void OrderBook::match_buy_order(Order &buy_order) {
  while (buy_order.remaining_qty > 0 && !asks_.empty()) {
    Order best_ask = asks_.top();

    if (buy_order.price >= best_ask.price) {
      int trade_qty = std::min(buy_order.remaining_qty, best_ask.remaining_qty);
      double trade_price = best_ask.price;

      fills_.emplace_back(buy_order.id, best_ask.id, trade_price, trade_qty);

      buy_order.remaining_qty -= trade_qty;
      best_ask.remaining_qty -= trade_qty;

      asks_.pop();

      if (best_ask.remaining_qty > 0) {
        asks_.push(best_ask);
      }
    } else {
      break;
    }
  }

  if (buy_order.remaining_qty > 0) {
    bids_.push(buy_order);
  }
}

void OrderBook::match_sell_order(Order &sell_order) {
  while (sell_order.remaining_qty > 0 && !bids_.empty()) {
    Order best_bid = bids_.top();

    if (sell_order.price <= best_bid.price) {
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
    } else {
      break;
    }
  }

  if (sell_order.remaining_qty > 0) {
    asks_.push(sell_order);
  }
}

void OrderBook::add_order(Order o) {
  Timer timer;
  timer.start();

  Order order = o;

  if (order.side == Side::BUY) {
    match_buy_order(order);
  } else if (order.side == Side::SELL) {
    match_sell_order(order);
  } else {
    throw std::runtime_error("Invalid order side");
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