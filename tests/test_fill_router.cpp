#include "fill_router.hpp"
#include "order.hpp"

#include <gtest/gtest.h>

namespace {

Order make_limit_order(int id, int account_id, Side side, double price,
                       int qty) {
  return Order(id, account_id, side, price, qty);
}

Order make_market_order(int id, int account_id, Side side, int qty) {
  return Order(id, account_id, side, OrderType::MARKET, qty,
               TimeInForce::IOC);
}

} // namespace

TEST(FillRouterTest, RoutesFillAndInvokesCallbacks) {
  FillRouter router(true);

  bool callback_invoked = false;
  router.register_fill_callback(
      [&](const EnhancedFill &enhanced) {
        (void)enhanced;
        callback_invoked = true;
      });

  Fill fill(1, 2, 101.25, 75);
  Order aggressive = make_limit_order(10, 1001, Side::BUY, 101.50, 75);
  Order passive = make_limit_order(11, 2002, Side::SELL, 101.25, 75);

  bool accepted = router.route_fill(fill, aggressive, passive, "TEST");
  ASSERT_TRUE(accepted);
  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(router.get_total_fills(), 1u);
  EXPECT_EQ(router.get_self_trades_prevented(), 0u);

  const auto &fills = router.get_all_fills();
  ASSERT_EQ(fills.size(), 1u);
  const auto &enhanced = fills.front();
  EXPECT_EQ(enhanced.buy_account_id, 1001);
  EXPECT_EQ(enhanced.sell_account_id, 2002);
  EXPECT_EQ(enhanced.symbol, "TEST");
  EXPECT_EQ(enhanced.base_fill.price, 101.25);
  EXPECT_EQ(enhanced.base_fill.quantity, 75);
  EXPECT_TRUE(enhanced.is_aggressive_buy);

  auto account_fills = router.get_fills_for_account(1001);
  EXPECT_EQ(account_fills.size(), 1u);

  auto symbol_fills = router.get_fills_for_symbol("TEST");
  EXPECT_EQ(symbol_fills.size(), 1u);

  EnhancedFill *by_id = router.get_fill_by_id(enhanced.fill_id);
  ASSERT_NE(by_id, nullptr);
  EXPECT_EQ(by_id->fill_id, enhanced.fill_id);
}

TEST(FillRouterTest, PreventsSelfTradeAndInvokesCallback) {
  FillRouter router(true);

  int prevented_account = -1;
  router.register_self_trade_callback(
      [&](int account_id, const Order &/*o1*/, const Order &/*o2*/) {
        prevented_account = account_id;
      });

  Fill fill(1, 2, 100.0, 10);
  Order aggressive = make_limit_order(20, 5001, Side::SELL, 99.9, 10);
  Order passive = make_limit_order(21, 5001, Side::BUY, 100.0, 10);

  bool accepted = router.route_fill(fill, aggressive, passive, "SELF");
  EXPECT_FALSE(accepted);
  EXPECT_EQ(prevented_account, 5001);
  EXPECT_EQ(router.get_self_trades_prevented(), 1u);
  EXPECT_EQ(router.get_total_fills(), 0u);
  EXPECT_TRUE(router.get_all_fills().empty());
}

TEST(FillRouterTest, AppliesFeeScheduleForMakerAndTaker) {
  FillRouter router(false);
  router.set_fee_schedule(0.0005, 0.0010); // maker 5 bps, taker 10 bps

  Fill fill(3, 4, 250.50, 200);
  Order aggressive = make_market_order(30, 7777, Side::BUY, 200);
  Order passive = make_limit_order(31, 8888, Side::SELL, 250.50, 200);

  ASSERT_TRUE(router.route_fill(fill, aggressive, passive, "FEE"));
  const auto &enhanced = router.get_all_fills().front();

  double notional = 250.50 * 200;
  EXPECT_DOUBLE_EQ(enhanced.buyer_fee, notional * 0.0010);
  EXPECT_DOUBLE_EQ(enhanced.seller_fee, notional * 0.0005);
  EXPECT_EQ(enhanced.liquidity_flag, EnhancedFill::LiquidityFlag::TAKER);
}

