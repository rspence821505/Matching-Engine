#include "market_data_generator.hpp"

#include "order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
constexpr double kMinPrice = 0.01;
}

MarketDataGenerator::MarketDataGenerator()
    : MarketDataGenerator(Config()) {}

MarketDataGenerator::MarketDataGenerator(const Config &config)
    : config_(config), rng_(config.seed), price_noise_(0.0, config.volatility),
      size_dist_(config.min_size, config.max_size), uniform01_(0.0, 1.0),
      last_mid_(config.start_price), next_order_id_(100000) {}

void MarketDataGenerator::reset(double price) {
  last_mid_ = std::max(price, kMinPrice);
  resting_orders_.clear();
  next_order_id_ = 100000;
}

int MarketDataGenerator::generate_quantity() {
  int qty = static_cast<int>(std::round(size_dist_(rng_)));
  return std::max(qty, 1);
}

MarketDataSnapshot MarketDataGenerator::next_snapshot() {
  double shock = price_noise_(rng_);
  last_mid_ = std::max(kMinPrice, last_mid_ + config_.drift + shock);
  double half_spread = config_.spread * 0.5;

  MarketDataSnapshot snapshot;
  snapshot.symbol = config_.symbol;
  snapshot.last_price = last_mid_;
  snapshot.bid_price = std::max(kMinPrice, last_mid_ - half_spread);
  snapshot.ask_price = std::max(snapshot.bid_price + config_.tick_size,
                                last_mid_ + half_spread);
  snapshot.bid_size = static_cast<double>(generate_quantity());
  snapshot.ask_size = static_cast<double>(generate_quantity());
  snapshot.spread = snapshot.ask_price - snapshot.bid_price;
  snapshot.timestamp = Clock::now();

  return snapshot;
}

std::vector<MarketDataSnapshot>
MarketDataGenerator::generate_series(std::size_t steps) {
  std::vector<MarketDataSnapshot> result;
  result.reserve(steps);
  for (std::size_t i = 0; i < steps; ++i) {
    result.push_back(next_snapshot());
    emit_snapshot(result.back());
  }
  return result;
}

void MarketDataGenerator::register_callback(SnapshotCallback callback) {
  callbacks_.push_back(std::move(callback));
}

void MarketDataGenerator::clear_callbacks() { callbacks_.clear(); }

void MarketDataGenerator::emit_snapshot(
    const MarketDataSnapshot &snapshot) {
  for (const auto &callback : callbacks_) {
    callback(snapshot);
  }
}

void MarketDataGenerator::submit_liquidity(
    OrderBook &book, const MarketDataSnapshot &snapshot) {
  for (std::size_t level = 0; level < config_.depth_levels; ++level) {
    double price_offset = static_cast<double>(level) * config_.tick_size;

    double bid_price =
        std::max(kMinPrice, snapshot.bid_price - price_offset);
    double ask_price =
        std::max(kMinPrice, snapshot.ask_price + price_offset);

    int bid_qty = generate_quantity();
    int ask_qty = generate_quantity();

    Order bid_order(next_order_id_++, config_.maker_buy_account +
                                        static_cast<int>(level),
                    Side::BUY, bid_price, bid_qty);
    Order ask_order(next_order_id_++, config_.maker_sell_account +
                                        static_cast<int>(level),
                    Side::SELL, ask_price, ask_qty);

    book.add_order(bid_order);
    book.add_order(ask_order);

    resting_orders_.push_back(bid_order.id);
    resting_orders_.push_back(ask_order.id);
  }

  while (resting_orders_.size() > 400) {
    int order_id = resting_orders_.front();
    resting_orders_.pop_front();
    book.cancel_order(order_id);
  }
}

void MarketDataGenerator::maybe_cancel_resting(OrderBook &book,
                                               double probability) {
  if (resting_orders_.empty()) {
    return;
  }

  if (uniform01_(rng_) < probability) {
    int order_id = resting_orders_.front();
    resting_orders_.pop_front();
    if (!book.cancel_order(order_id)) {
      // If cancellation fails (already filled), ignore.
    }
  }
}

void MarketDataGenerator::maybe_submit_market(OrderBook &book,
                                              double probability) {
  if (uniform01_(rng_) > probability) {
    return;
  }

  Side side = (uniform01_(rng_) < 0.5) ? Side::BUY : Side::SELL;
  int account = (side == Side::BUY) ? config_.taker_buy_account
                                    : config_.taker_sell_account;
  int quantity = generate_quantity();

  Order market_order(next_order_id_++, account, side, OrderType::MARKET,
                     quantity, TimeInForce::IOC);
  book.add_order(market_order);
}

void MarketDataGenerator::step(OrderBook *book,
                               double market_order_probability) {
  MarketDataSnapshot snapshot = next_snapshot();
  emit_snapshot(snapshot);

  if (!book) {
    return;
  }

  book->set_symbol(config_.symbol);
  submit_liquidity(*book, snapshot);
  maybe_cancel_resting(*book, 0.1);
  maybe_submit_market(*book, market_order_probability);
}

void MarketDataGenerator::inject_self_trade(OrderBook &book, int account_id,
                                            double price, int quantity) {
  double trade_price = std::max(price, kMinPrice);

  Order maker(next_order_id_++, account_id, Side::SELL, trade_price,
              quantity);
  book.add_order(maker);

  Order taker(next_order_id_++, account_id, Side::BUY, trade_price,
              quantity);
  book.add_order(taker);
}
