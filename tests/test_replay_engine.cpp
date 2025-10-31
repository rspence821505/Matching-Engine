// tests/test_replay_engine.cpp
#include "replay_engine.hpp"
#include "test_helpers.hpp"
#include <filesystem>

class ReplayTest : public ::testing::Test {
protected:
  const std::string events_file = "replay_test_events.csv";
  std::unique_ptr<OrderBook> book;
  std::unique_ptr<ReplayEngine> replay;

  void SetUp() override {
    book = std::make_unique<OrderBook>();
    replay = std::make_unique<ReplayEngine>();
  }

  void TearDown() override { std::filesystem::remove(events_file); }
};

TEST_F(ReplayTest, ReplayDeterminism) {
  book->enable_logging();

  // Execute some orders
  book->add_order(Order{1, Side::BUY, 100.0, 100});
  book->add_order(Order{2, Side::SELL, 100.0, 50});
  book->add_order(Order{3, Side::SELL, 100.0, 50});

  auto original_fills = book->get_fills();

  // Save and replay
  book->save_events(events_file);

  replay->load_from_file(events_file);
  replay->replay_instant();

  // Verify exact match
  const auto &replayed_fills = replay->get_book().get_fills();

  ASSERT_EQ(replayed_fills.size(), original_fills.size());

  for (size_t i = 0; i < original_fills.size(); ++i) {
    EXPECT_EQ(replayed_fills[i].buy_order_id, original_fills[i].buy_order_id);
    EXPECT_EQ(replayed_fills[i].sell_order_id, original_fills[i].sell_order_id);
    EXPECT_DOUBLE_EQ(replayed_fills[i].price, original_fills[i].price);
    EXPECT_EQ(replayed_fills[i].quantity, original_fills[i].quantity);
  }
}

TEST_F(ReplayTest, ReplayManualControl) {
  book->enable_logging();

  book->add_order(Order{1, Side::BUY, 100.0, 100});
  book->add_order(Order{2, Side::SELL, 100.0, 100});

  book->save_events(events_file);

  replay->load_from_file(events_file);

  EXPECT_TRUE(replay->has_next_event());
  EXPECT_EQ(static_cast<int>(replay->get_current_index()), 0);

  replay->replay_next_event();
  EXPECT_EQ(replay->get_current_index(), 1);

  replay->replay_next_event();
  EXPECT_EQ(replay->get_current_index(), 2);
}

TEST_F(ReplayTest, ReplayValidation) {
  book->enable_logging();

  book->add_order(Order{1, Side::BUY, 100.0, 100});
  book->add_order(Order{2, Side::SELL, 100.0, 100});

  auto original_fills = book->get_fills();
  book->save_events(events_file);

  replay->load_from_file(events_file);
  replay->replay_instant();

  // Should validate successfully
  EXPECT_NO_THROW(replay->validate_against_original(original_fills));
}