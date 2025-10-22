#ifndef ORDER_HPP
#define ORDER_HPP

#include "types.hpp"
#include <iostream>
#include <string>

struct Order {
  int id;
  Side side;
  double price;
  int quantity;
  int remaining_qty;
  TimePoint timestamp;

  Order(int id_, Side side_, double price_, int qty_);

  bool is_filled() const;
  std::string side_to_string() const;

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