#include "order.hpp"

// Constructor for LIMIT orders
Order::Order(int id_, Side side_, double price_, int qty_, TimeInForce tif_)
    : id(id_), side(side_), type(OrderType::LIMIT), tif(tif_), price(price_),
      quantity(qty_), remaining_qty(qty_),
      display_qty(qty_), // Normal orders: all quanity visible
      hidden_qty(0),     // No hidden quantity
      peak_size(0),      // Not an iceberg
      timestamp(Clock::now()), state(OrderState::PENDING) {}

// Constructor for MARKET Orders
Order::Order(int id_, Side side_, OrderType type_, int qty_, TimeInForce tif_)
    : id(id_), side(side_), type(type_), tif(tif_),
      price(side == Side::BUY
                ? std::numeric_limits<double>::infinity()
                : 0.0), // Market buy = infinite price, market sell = 0 price
      quantity(qty_), remaining_qty(qty_),
      display_qty(qty_), // Market orders; all visible
      hidden_qty(0), peak_size(0), timestamp(Clock::now()),
      state(OrderState::PENDING) {
  if (type_ != OrderType::MARKET) {
    throw std::runtime_error("Use the other constructor for limit orders");
  }

  // Market orders default to IOC if GTC specified
  if (tif == TimeInForce::GTC) {
    tif = TimeInForce::IOC;
  }
}

// Constructor for ICEBERG Orders
Order::Order(int id_, Side side_, double price_, int total_qty, int peak_size_,
             TimeInForce tif_)
    : id(id_), side(side_), type(OrderType::LIMIT), tif(tif_), price(price_),
      quantity(total_qty), remaining_qty(total_qty),
      display_qty(std::min(peak_size_, total_qty)),    // show first peak
      hidden_qty(std::max(0, total_qty - peak_size_)), // rest is hidden
      peak_size(peak_size_),                           // Not an iceberg
      timestamp(Clock::now()), state(OrderState::PENDING) {

  if (peak_size_ <= 0) {
    throw std::runtime_error("Peak size must be positive");
  }
  if (peak_size_ > total_qty) {
    // If peak >= total, it's just a regular order
    display_qty = total_qty;
    hidden_qty = 0;
    peak_size = 0;
  }
}

bool Order::is_filled() const {
  return remaining_qty == 0 || state == OrderState::FILLED;
}

bool Order::is_active() const {
  return state == OrderState::ACTIVE || state == OrderState::PARTIALLY_FILLED;
}

bool Order::is_market_order() const { return type == OrderType::MARKET; }

bool Order::is_iceberg() const { return peak_size > 0 && hidden_qty > 0; }

bool Order::can_rest_in_book() const {
  // Only GTC and DAY orders can rest in the book
  // IOC and FOK must execute immediately or cancel
  return tif == TimeInForce::GTC || tif == TimeInForce::DAY;
}

bool Order::needs_refresh() const {
  // Needs refresh if display is exhausted but we have hidden quantity
  return display_qty == 0 && hidden_qty > 0;
}

void Order::refresh_display() {
  if (hidden_qty > 0) {
    int reveal = std::min(peak_size, hidden_qty);
    display_qty = reveal;
    hidden_qty -= reveal;
    timestamp = Clock::now(); // Loses time priority!

    std::cout << "Iceberg order " << id << " refreshed: showing " << display_qty
              << " more shares (" << hidden_qty << " still hidden)"
              << std::endl;
  }
}

std::string Order::side_to_string() const {
  return side == Side::BUY ? "BUY" : "SELL";
}

std::string Order::type_to_string() const {
  return type == OrderType::LIMIT ? "LIMIT" : "MARKET";
}

std::string Order::tif_to_string() const {
  switch (tif) {
  case TimeInForce::GTC:
    return "GTC";
  case TimeInForce::IOC:
    return "IOC";
  case TimeInForce::FOK:
    return "FOK";
  case TimeInForce::DAY:
    return "DAY";
  default:
    return "UNKNOWN";
  }
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
     << ", side=" << o.side_to_string() << ", tif=" << o.tif_to_string()
     << ", price=";

  if (o.is_market_order()) {
    os << "MARKET";
  } else {
    os << o.price;
  }

  os << ", qty=" << o.remaining_qty << "/" << o.quantity;

  // Show iceberg info if applicable
  if (o.is_iceberg() || (o.peak_size > 0)) {
    os << " [ICEBERG: display=" << o.display_qty << ", hidden=" << o.hidden_qty
       << "]";
  }

  os << ", state=" << o.state_to_string()
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