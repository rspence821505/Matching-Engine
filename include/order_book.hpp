#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "fill.hpp"
#include "order.hpp"
#include "timer.hpp"
#include <map>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

class OrderBook {
private:
  std::priority_queue<Order, std::vector<Order>, BidComparator> bids_;
  std::priority_queue<Order, std::vector<Order>, AskComparator> asks_;
  std::vector<Fill> fills_;
  std::vector<long long> insertion_latencies_ns_;

  // Order tracking for cancellation and amendment
  std::unordered_map<int, Order> active_orders_;    // id -> order
  std::unordered_map<int, Order> cancelled_orders_; // id -> order

  void match_buy_order(Order &buy_order);
  void match_sell_order(Order &sell_order);
  bool can_fill_order(const Order &order) const;

public:
  OrderBook();

  // Operational methods
  void add_order(Order o);
  std::optional<Order> get_best_bid() const;
  std::optional<Order> get_best_ask() const;
  std::optional<double> get_spread() const;
  const std::vector<Fill> &get_fills() const;

  // Order lifecycle management methods
  bool cancel_order(int order_id);
  bool amend_order(int order_id, std::optional<double> new_price,
                   std::optional<int> new_quantity);
  std::optional<Order> get_order(int order_id) const;

  // Statistics and display methods
  void print_fills() const;
  void print_top_of_book() const;
  void print_latency_stats() const;
  void print_match_stats() const;
  void print_fill_rate_analysis() const;
  void print_book_summary() const;
  void print_trade_timeline() const;
  void print_order_status(int order_id) const;

  size_t bids_size() const { return bids_.size(); }
  size_t asks_size() const { return asks_.size(); }
};

#endif // ORDER_BOOK_HPP