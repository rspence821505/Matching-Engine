#include "order_book.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Testing Matching Engine with Performance Tracking ==="
            << std::endl
            << std::endl;

  OrderBook book;

  // Test 1: Build the book (no matches yet)
  std::cout << "Test 1: Building initial book (no crosses)" << std::endl;
  book.add_order(Order{1, Side::BUY, 100.00, 100});
  book.add_order(Order{2, Side::BUY, 99.75, 200});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.75, 100});
  book.print_book_summary();

  // Test 2: Aggressive buy that matches
  std::cout << "\nTest 2: Aggressive buy order (crosses spread)" << std::endl;
  std::cout << "Adding: BUY 120 @ $101.00 (should match with asks)"
            << std::endl;
  book.add_order(Order{5, Side::BUY, 101.00, 120});
  book.print_book_summary();

  // Test 3: Aggressive sell that matches
  std::cout << "\nTest 3: Aggressive sell order" << std::endl;
  std::cout << "Adding: SELL 80 @ $99.50 (should match with bids)" << std::endl;
  book.add_order(Order{6, Side::SELL, 99.50, 80});
  book.print_book_summary();

  // Test 4: Partial fill scenario
  std::cout << "\nTest 4: Large order (should generate multiple fills)"
            << std::endl;
  std::cout << "Adding: BUY 500 @ $102.00 (big aggressive order)" << std::endl;
  book.add_order(Order{7, Side::BUY, 102.00, 500});
  book.print_book_summary();

  // Test 5: Add more liquidity
  std::cout << "\nTest 5: Adding more orders to both sides" << std::endl;
  book.add_order(Order{8, Side::BUY, 101.50, 300});
  book.add_order(Order{9, Side::SELL, 102.50, 250});
  book.add_order(Order{10, Side::BUY, 101.00, 400});
  book.print_book_summary();

  // Print comprehensive statistics
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "FINAL PERFORMANCE REPORT" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  book.print_trade_timeline();
  book.print_match_stats();
  book.print_fill_rate_analysis();

  return 0;
}