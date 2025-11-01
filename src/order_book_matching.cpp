#include "order_book.hpp"

#include <algorithm>
#include <iostream>

void OrderBook::finalize_after_matching(Order &o) {
  // If already terminal, do not overwrite
  auto it = active_orders_.find(o.id);
  if (it != active_orders_.end()) {
    if (it->second.state == OrderState::CANCELLED ||
        it->second.state == OrderState::FILLED) {
      return;
    }
  }

  if (o.tif == TimeInForce::IOC) {
    if (o.remaining_qty > 0) {
      // Partial or zero fill with remainder -> CANCELLED
      o.state = OrderState::CANCELLED;
      if (it != active_orders_.end()) {
        it->second.state = OrderState::CANCELLED;
        it->second.remaining_qty = 0;
      }
    } else {
      o.state = OrderState::FILLED;
      if (it != active_orders_.end())
        it->second.state = OrderState::FILLED;
    }
    return;
  }

  // FOK handled in check_fok_condition()

  // GTC/DAY/etc. bookkeeping
  if (o.remaining_qty == 0) {
    o.state = OrderState::FILLED;
    if (it != active_orders_.end())
      it->second.state = OrderState::FILLED;
  } else if (o.remaining_qty < o.quantity) {
    o.state = OrderState::PARTIALLY_FILLED;
    if (it != active_orders_.end())
      it->second.state = OrderState::PARTIALLY_FILLED;
  }
}

bool OrderBook::can_fill_order(const Order &order) const {
  int available_qty = 0;

  if (order.side == Side::BUY) {
    auto asks_copy = asks_;
    while (!asks_copy.empty() && available_qty < order.quantity) {
      Order best_ask = asks_copy.top();
      asks_copy.pop();

      if (!can_match(order, best_ask)) {
        break;
      }

      available_qty += best_ask.remaining_qty;
    }
  } else {
    auto bids_copy = bids_;
    while (!bids_copy.empty() && available_qty < order.quantity) {
      Order best_bid = bids_copy.top();
      bids_copy.pop();

      if (!can_match(order, best_bid)) {
        break;
      }

      available_qty += best_bid.remaining_qty;
    }
  }
  return available_qty >= order.quantity;
}

void OrderBook::execute_trade(Order &aggressive_order, Order &passive_order) {
  // Determine available quantity (respect iceberg display limits)
  int available_qty = passive_order.is_iceberg() ? passive_order.display_qty
                                                 : passive_order.remaining_qty;

  int trade_qty = std::min(aggressive_order.remaining_qty, available_qty);
  double trade_price = passive_order.price; // Passive order sets price

  // Determine buy/sell order IDs based on sides
  int buy_id = (aggressive_order.side == Side::BUY) ? aggressive_order.id
                                                    : passive_order.id;
  int sell_id = (aggressive_order.side == Side::SELL) ? aggressive_order.id
                                                      : passive_order.id;

  // NEW: Extract account IDs
  int buy_account = (aggressive_order.side == Side::BUY)
                        ? aggressive_order.account_id
                        : passive_order.account_id;
  int sell_account = (aggressive_order.side == Side::SELL)
                         ? aggressive_order.account_id
                         : passive_order.account_id;

  // Record the fill
  fills_.emplace_back(buy_id, sell_id, trade_price, trade_qty);

  // NEW: Also record with account information
  account_fills_.emplace_back(fills_.back(), buy_account, sell_account,
                              current_symbol_);

  // Log fill event
  if (logging_enabled_) {
    event_log_.emplace_back(Clock::now(), buy_id, sell_id, trade_price,
                            trade_qty, buy_account);
  }

  // Update quantities
  aggressive_order.remaining_qty -= trade_qty;
  passive_order.remaining_qty -= trade_qty;

  if (passive_order.is_iceberg()) {
    passive_order.display_qty -= trade_qty;
  }

  // Check for stop order triggers
  check_stop_triggers(trade_price);
}

void OrderBook::update_order_state(Order &order) {
  auto it = active_orders_.find(order.id);
  if (it == active_orders_.end()) {
    return;
  }

  // Update quantities
  it->second.remaining_qty = order.remaining_qty;

  if (order.is_iceberg()) {
    it->second.display_qty = order.display_qty;
    it->second.hidden_qty = order.hidden_qty;
  }

  // Update state
  if (order.is_filled()) {
    it->second.state = OrderState::FILLED;
  } else if (order.remaining_qty < order.quantity) {
    it->second.state = OrderState::PARTIALLY_FILLED;
  }
}

bool OrderBook::can_match(const Order &aggressive, const Order &passive) const {
  if (aggressive.is_market_order()) {
    return true;
  }

  if (aggressive.side == Side::BUY) {
    return aggressive.price >= passive.price;
  } else {
    return aggressive.price <= passive.price;
  }
}

void OrderBook::handle_unfilled_order(
    Order &order,
    std::priority_queue<Order, std::vector<Order>, BidComparator> *bid_book,
    std::priority_queue<Order, std::vector<Order>, AskComparator> *ask_book) {

  if (order.remaining_qty == 0) {
    return;
  }

  if (order.can_rest_in_book()) {
    if (order.side == Side::BUY && bid_book) {
      bid_book->push(order);
    } else if (order.side == Side::SELL && ask_book) {
      ask_book->push(order);
    }
    return;
  }

  // Order cannot rest - cancel it
  auto it = active_orders_.find(order.id);
  if (it != active_orders_.end()) {
    it->second.state = OrderState::CANCELLED;
    it->second.remaining_qty = order.remaining_qty;

    // Update the order reference too
    order.state = OrderState::CANCELLED;
  }

  if (order.tif == TimeInForce::IOC) {
    int filled = order.quantity - order.remaining_qty;
    if (filled > 0) {
      std::cout << "IOC order " << order.id << " partially filled (" << filled
                << "/" << order.quantity << "), remaining cancelled"
                << std::endl;
    } else {
      std::cout << "IOC order " << order.id
                << " cancelled (no immediate liquidity)" << std::endl;
    }
  }
}

bool OrderBook::check_fok_condition(const Order &order) {
  if (order.tif != TimeInForce::FOK) {
    return true; // Not FOK, proceed
  }

  if (can_fill_order(order)) {
    return true; // FOK can be filled, proceed
  }

  // FOK cannot be filled - cancel it
  auto it = active_orders_.find(order.id);
  if (it != active_orders_.end()) {
    it->second.state = OrderState::CANCELLED;
  }

  std::cout << "FOK order " << order.id
            << " cancelled (insufficient liquidity to fill " << order.quantity
            << " shares)" << std::endl;

  return false; // Don't proceed with matching
}

void OrderBook::match_buy_order(Order &buy_order) {
  if (!check_fok_condition(buy_order)) {
    return;
  }

  while (buy_order.remaining_qty > 0 && !asks_.empty()) {
    Order best_ask = asks_.top();
    asks_.pop();

    // CRITICAL: Get the current state from active_orders_
    auto it = active_orders_.find(best_ask.id);
    if (it == active_orders_.end() ||
        it->second.state == OrderState::CANCELLED ||
        it->second.state == OrderState::FILLED) {
      continue; // Skip cancelled/filled orders
    }

    // Use the LATEST state from active_orders_
    best_ask = it->second;

    // CRITICAL: Skip if order has no visible quantity
    // This prevents processing stale copies that haven't refreshed yet
    if (best_ask.display_qty == 0 && best_ask.remaining_qty > 0) {
      // This is a stale copy - the real order is being refreshed
      // Don't process this copy, continue to next order in queue
      continue;
    }

    if (!can_match(buy_order, best_ask)) {
      asks_.push(best_ask); // Put it back
      break;
    }

    // Execute trade
    execute_trade(buy_order, best_ask);

    // Update states in active_orders_
    update_order_state(buy_order);
    update_order_state(best_ask);

    // Check if we need to refresh iceberg
    if (best_ask.needs_refresh()) {
      best_ask.refresh_display();
      // Update the refreshed order in active_orders_
      auto &stored_order = active_orders_.at(best_ask.id);
      stored_order.display_qty = best_ask.display_qty;
      stored_order.hidden_qty = best_ask.hidden_qty;
      stored_order.timestamp = best_ask.timestamp;

      // Push refreshed order back to book
      asks_.push(best_ask);
    } else if (best_ask.remaining_qty > 0 && best_ask.display_qty > 0) {
      // Still has quantity, re-add to book
      asks_.push(best_ask);
    }
    // If remaining_qty == 0 or display_qty == 0 (and no hidden), don't re-add
  }

  handle_unfilled_order(buy_order, &bids_, nullptr);
}

void OrderBook::match_sell_order(Order &sell_order) {
  if (!check_fok_condition(sell_order)) {
    return;
  }

  while (sell_order.remaining_qty > 0 && !bids_.empty()) {
    Order best_bid = bids_.top();
    bids_.pop();

    // CRITICAL: Get current state
    auto it = active_orders_.find(best_bid.id);
    if (it == active_orders_.end() ||
        it->second.state == OrderState::CANCELLED ||
        it->second.state == OrderState::FILLED) {
      continue;
    }

    // Use latest state
    best_bid = it->second;

    // Skip stale copies with no display quantity
    if (best_bid.display_qty == 0 && best_bid.remaining_qty > 0) {
      continue;
    }

    if (!can_match(sell_order, best_bid)) {
      bids_.push(best_bid);
      break;
    }

    // Execute trade
    execute_trade(sell_order, best_bid);

    // Update states
    update_order_state(sell_order);
    update_order_state(best_bid);

    // Check if we need to refresh iceberg
    if (best_bid.needs_refresh()) {
      best_bid.refresh_display();
      // Update the refreshed order in active_orders_
      auto &stored_order = active_orders_.at(best_bid.id);
      stored_order.display_qty = best_bid.display_qty;
      stored_order.hidden_qty = best_bid.hidden_qty;
      stored_order.timestamp = best_bid.timestamp;

      // Push refreshed order back to book
      bids_.push(best_bid);
    } else if (best_bid.remaining_qty > 0 && best_bid.display_qty > 0) {
      // Still has quantity, re-add to book
      bids_.push(best_bid);
    }
  }

  handle_unfilled_order(sell_order, nullptr, &asks_);
}
