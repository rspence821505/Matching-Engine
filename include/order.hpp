#ifndef ORDER_HPP
#define ORDER_HPP

#include "types.hpp"
#include <iostream>
#include <string>

struct Order {
  int id;
  Side side;
  OrderType type; // LIMIT or MARKET
  double price;   // For Limit orders (infinity for market)
  int quantity;
  int remaining_qty;
  TimePoint timestamp;
  OrderState state;

  // Constructor for LIMIT orders
  Order(int id_, Side side_, double price_, int qty_);

  // Constructor for MARKET orders
  Order(int id_, Side side_, OrderType type_, int qty_);

  bool is_filled() const;
  bool is_active() const;
  bool is_market_order() const;
  std::string side_to_string() const;
  std::string type_to_string() const;
  std::string state_to_string() const;

  friend std::ostream &operator<<(std::ostream &os, const Order &o);
};

// Comparators
struct BidComparator {
  bool operator()(const Order &a, const Order &b) const;
};

struct AskComparator {
  bool operator()(const Order &a, const Order &b) const;
};

#endif // ORDER_HPP