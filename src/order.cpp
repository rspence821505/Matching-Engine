#include "order.hpp"

// Constructor for LIMIT orders
Order::Order(int id_, Side side_, double price_, int qty_)
    : id(id_), side(side_), type(OrderType::LIMIT), price(price_),
      quantity(qty_), remaining_qty(qty_), timestamp(Clock::now()),
      state(OrderState::PENDING) {}

// Constructor for MARKET Orders
Order::Order(int id_, Side side_, OrderType type_, int qty_)
    : id(id_), side(side_), type(type_),
      price(side == Side::BUY
                ? std::numeric_limits<double>::infinity()
                : 0.0), // Market buy = infinite price, market sell = 0 price
      quantity(qty_), remaining_qty(qty_), timestamp(Clock::now()),
      state(OrderState::PENDING) {
  if (type_ != OrderType::MARKET) {
    throw std::runtime_error("Use the other constructor for limit orders");
  }
}

bool Order::is_filled() const {
  return remaining_qty == 0 || state == OrderState::FILLED;
}

bool Order::is_active() const {
  return state == OrderState::ACTIVE || state == OrderState::PARTIALLY_FILLED;
}

bool Order::is_market_order() const { return type == OrderType::MARKET; }

std::string Order::side_to_string() const {
  return side == Side::BUY ? "BUY" : "SELL";
}

std::string Order::type_to_string() const {
  return type == OrderType::LIMIT ? "LIMIT" : "MARKET";
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
  os << "Order{id=" << o.id << ", type=" << o.type_to_string()
     << ", side=" << o.side_to_string() << ", price=";

  if (o.is_market_order()) {
    os << "MARKET";
  } else {
    os << o.price;
  }

  os << ", qty=" << o.remaining_qty << "/" << o.quantity
     << ", state=" << o.state_to_string()
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