#include "market_data_generator.hpp"
#include "order_book.hpp"

#include <gtest/gtest.h>

namespace {

MarketDataGenerator::Config base_config() {
  MarketDataGenerator::Config cfg;
  cfg.symbol = "UNIT";
  cfg.start_price = 50.0;
  cfg.volatility = 0.25;
  cfg.spread = 0.04;
  cfg.depth_levels = 2;
  cfg.seed = 42;
  cfg.maker_buy_account = 8001;
  cfg.maker_sell_account = 8002;
  cfg.taker_buy_account = 9001;
  cfg.taker_sell_account = 9002;
  return cfg;
}

} // namespace

TEST(MarketDataGeneratorTest, GeneratesSnapshotsAndCallbacks) {
  auto cfg = base_config();
  MarketDataGenerator generator(cfg);

  int callback_count = 0;
  generator.register_callback(
      [&](const MarketDataSnapshot &snapshot) {
        ++callback_count;
        EXPECT_EQ(snapshot.symbol, cfg.symbol);
        EXPECT_GT(snapshot.ask_price, snapshot.bid_price);
        EXPECT_GE(snapshot.spread, cfg.tick_size);
      });

  auto series = generator.generate_series(5);
  EXPECT_EQ(series.size(), 5u);
  EXPECT_EQ(callback_count, 5);

  for (const auto &snap : series) {
    EXPECT_EQ(snap.symbol, cfg.symbol);
    EXPECT_GT(snap.ask_price, snap.bid_price);
    EXPECT_GE(snap.spread, cfg.tick_size);
  }
}

TEST(MarketDataGeneratorTest, StepAddsLiquidityToOrderBook) {
  auto cfg = base_config();
  MarketDataGenerator generator(cfg);
  OrderBook book(cfg.symbol);

  // No orders initially
  EXPECT_EQ(book.bids_size(), 0u);
  EXPECT_EQ(book.asks_size(), 0u);

  generator.step(&book, 0.0); // only seed liquidity, no market trades

  EXPECT_GT(book.bids_size(), 0u);
  EXPECT_GT(book.asks_size(), 0u);

  generator.step(&book, 0.0);
  EXPECT_GT(book.bids_size(), 0u);
  EXPECT_GT(book.asks_size(), 0u);
}

TEST(MarketDataGeneratorTest, InjectSelfTradeRespectsRouterSetting) {
  auto cfg = base_config();
  MarketDataGenerator generator(cfg);
  OrderBook book(cfg.symbol);

  FillRouter &router = book.get_fill_router();
  router.set_self_trade_prevention(true);

  // Seed book so there is resting liquidity
  generator.step(&book, 0.0);
  std::uint64_t prevented_before = router.get_self_trades_prevented();

  generator.inject_self_trade(book, cfg.maker_buy_account,
                              generator.current_mid(), 40);

  EXPECT_GT(router.get_self_trades_prevented(), prevented_before);

  router.set_self_trade_prevention(false);
  std::size_t fills_before = router.get_total_fills();
  generator.inject_self_trade(book, cfg.maker_buy_account + 5,
                              generator.current_mid(), 30);
  EXPECT_GT(router.get_total_fills(), fills_before);
}

