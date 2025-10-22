#include "order.hpp"

Order::Order(int id_, Side side_, double price_, int qty_)
    : id(id_), side(side_), price(price_), quantity(qty_), remaining_qty(qty_),
      timestamp(Clock::now()), state(OrderState::PENDING) {}

bool Order::is_filled() const {
  return remaining_qty == 0 || state == OrderState::FILLED;
}

bool Order::is_active() const {
  return state == OrderState::ACTIVE || state == OrderState::PARTIALLY_FILLED;
}

std::string Order::side_to_string() const {
  return side == Side::BUY ? "BUY" : "SELL";
}

std::string Order::state_to_string() const {
  switch (state) {
  case OrderState::PENDING:
    return "PENDING";
  case OrderState::ACTIVE:
    return "ACTIVE";
  case OrderState::PARTIALLY_FILLED:
    return "PARTIALLY_FILLED";
  case OrderState::FILLED:
    return "FILLED";
  case OrderState::CANCELLED:
    return "CANCELLED";
  case OrderState::REJECTED:
    return "REJECTED";
  default:
    return "UNKNOWN";
  }
}

std::ostream &operator<<(std::ostream &os, const Order &o) {
  os << "Order{id=" << o.id << ", side=" << o.side_to_string()
     << ", price=" << o.price << ", qty=" << o.remaining_qty << "/"
     << o.quantity << ", state-" << o.state_to_string()
     << ", ts=" << o.timestamp.time_since_epoch().count() << "}";
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