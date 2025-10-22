#include "order.hpp"

Order::Order(int id_, Side side_, double price_, int qty_)
    : id(id_), side(side_), price(price_), quantity(qty_), remaining_qty(qty_),
      timestamp(Clock::now()) {}

bool Order::is_filled() const { return remaining_qty == 0; }

std::string Order::side_to_string() const {
  return side == Side::BUY ? "BUY" : "SELL";
}

std::ostream &operator<<(std::ostream &os, const Order &o) {
  os << "Order{id=" << o.id << ", side=" << o.side_to_string()
     << ", price=" << o.price << ", qty=" << o.remaining_qty << "/"
     << o.quantity << ", ts=" << o.timestamp.time_since_epoch().count() << "}";
  return os;
}

bool BidComparator::operator()(const Order &a, const Order &b) const {
  if (a.price != b.price) {
    return a.price < b.price;
  }
  return a.timestamp > b.timestamp;
}

bool AskComparator::operator()(const Order &a, const Order &b) const {
  if (a.price != b.price) {
    return a.price > b.price;
  }
  return a.timestamp > b.timestamp;
}