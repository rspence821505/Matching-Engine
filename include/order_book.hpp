#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "fill.hpp"
#include "order.hpp"
#include "timer.hpp"
#include <optional>
#include <queue>
#include <vector>

class OrderBook {
private:
  std::priority_queue<Order, std::vector<Order>, BidComparator> bids_;
  std::priority_queue<Order, std::vector<Order>, AskComparator> asks_;
  std::vector<Fill> fills_;
  std::vector<long long> insertion_latencies_ns_;

  void match_buy_order(Order &buy_order);
  void match_sell_order(Order &sell_order);

public:
  OrderBook();

  void add_order(Order o);

  std::optional<Order> get_best_bid() const;
  std::optional<Order> get_best_ask() const;
  std::optional<double> get_spread() const;
  const std::vector<Fill> &get_fills() const;

  // Statistics and display methods
  void print_fills() const;
  void print_top_of_book() const;
  void print_latency_stats() const;
  void print_match_stats() const;
  void print_fill_rate_analysis() const;
  void print_book_summary() const;
  void print_trade_timeline() const;

  size_t bids_size() const { return bids_.size(); }
  size_t asks_size() const { return asks_.size(); }
};

#endif // ORDER_BOOK_HPP