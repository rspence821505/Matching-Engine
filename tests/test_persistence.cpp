// tests/test_persistence.cpp
#include "test_helpers.hpp"
#include <filesystem>

class PersistenceTest : public OrderBookTest {
protected:
  const std::string snapshot_file = "test_snapshot.txt";
  const std::string events_file = "test_events.csv";

  void TearDown() override {
    OrderBookTest::TearDown();
    // Clean up test files
    std::filesystem::remove(snapshot_file);
    std::filesystem::remove(events_file);
  }
};

TEST_F(PersistenceTest, SnapshotSaveAndLoad) {
  book->enable_logging();

  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);
  add_limit_order(3, Side::BUY, 101.0, 50);

  size_t original_fills = fill_count();

  // Save snapshot
  book->save_snapshot(snapshot_file);

  // Load into new book
  OrderBook recovered_book;
  recovered_book.load_snapshot(snapshot_file);

  // Verify state preserved
  EXPECT_EQ(recovered_book.get_fills().size(), original_fills);
  EXPECT_EQ(recovered_book.bids_size(), book->bids_size());
  EXPECT_EQ(recovered_book.asks_size(), book->asks_size());
}

TEST_F(PersistenceTest, EventLogging) {
  book->enable_logging();

  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 100.0, 100);

  EXPECT_GT(static_cast<int>(book->event_count()), 0);

  book->save_events(events_file);
  EXPECT_TRUE(std::filesystem::exists(events_file));
}

TEST_F(PersistenceTest, CheckpointRecovery) {
  book->enable_logging();

  add_limit_order(1, Side::BUY, 100.0, 100);
  add_limit_order(2, Side::SELL, 101.0, 100);

  size_t original_fills = fill_count();

  book->save_checkpoint(snapshot_file, events_file);

  // New book recovers from checkpoint
  OrderBook recovered_book;
  recovered_book.recover_from_checkpoint(snapshot_file, events_file);

  EXPECT_EQ(recovered_book.get_fills().size(), original_fills);
}

TEST_F(PersistenceTest, SnapshotWithIcebergOrders) {
  book->add_order(Order{1, Side::SELL, 100.0, 500, 100});

  book->save_snapshot(snapshot_file);

  OrderBook recovered_book;
  recovered_book.load_snapshot(snapshot_file);

  auto ask = recovered_book.get_best_ask();
  ASSERT_TRUE(ask.has_value());
  EXPECT_EQ(ask->quantity, 500);
  EXPECT_EQ(ask->display_qty, 100);
  EXPECT_EQ(ask->hidden_qty, 400);
}

TEST_F(PersistenceTest, SnapshotWithStopOrders) {
  book->add_order(Order{1, Side::SELL, 98.0, 100, true});

  book->save_snapshot(snapshot_file);

  OrderBook recovered_book;
  recovered_book.load_snapshot(snapshot_file);

  EXPECT_EQ(static_cast<int>(recovered_book.pending_stop_count()), 1);
}