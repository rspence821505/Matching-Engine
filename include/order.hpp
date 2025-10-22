#ifndef ORDER_HPP
#define ORDER_HPP

#include "types.hpp"
#include <iostream>
#include <string>

struct Order {
  int id;
  Side side;
  OrderType type;    // LIMIT or MARKET
  TimeInForce tif;   // Time-in-Force
  double price;      // For Limit orders (infinity for market)
  int quantity;      // Total original quantity
  int remaining_qty; // Total remaining (visible + hidden)
  int display_qty;   // Currently visible quantity
  int hidden_qty;    // Hidden reserve quantity
  int peak_size;     // How much to reveal at refresh
  TimePoint timestamp;
  OrderState state;

  // Constructor for LIMIT orders
  Order(int id_, Side side_, double price_, int qty_,
        TimeInForce tif_ = TimeInForce::GTC);

  // Constructor for MARKET orders
  Order(int id_, Side side_, OrderType type_, int qty_,
        TimeInForce tif_ = TimeInForce::IOC);

  // Constructor for ICEBERG orders
  Order(int id_, Side side_, double price_, int total_qty, int peak_size_,
        TimeInForce tif_ = TimeInForce::GTC);

  bool is_filled() const;
  bool is_active() const;
  bool is_market_order() const;
  bool is_iceberg() const;
  bool can_rest_in_book() const;
  bool needs_refresh() const; // check if display exhausted
  void refresh_display();     // reveal more quantity

  std::string side_to_string() const;
  std::string type_to_string() const;
  std::string tif_to_string() const;
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