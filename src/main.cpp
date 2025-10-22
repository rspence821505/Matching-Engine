#include "order_book.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Iceberg Orders ===" << std::endl << std::endl;

  OrderBook book;

  // Test 1: Build initial book
  std::cout << "Test 1: Building initial book" << std::endl;
  book.add_order(Order{1, Side::BUY, 100.00, 200});
  book.add_order(Order{2, Side::BUY, 99.75, 300});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.75, 200});
  book.print_market_depth(5);

  // Test 2: Add an iceberg order to sell side
  std::cout << "\nTest 2: Iceberg SELL 500 @ $100.50 (peak: 100)" << std::endl;
  book.add_order(Order{5, Side::SELL, 100.50, 500, 100}); // Total 500, show 100
  book.print_order_status(5);
  book.print_market_depth_compact();

  // Test 3: Aggressive buy that consumes first peak
  std::cout << "\nTest 3: BUY 100 @ $101.00 (consume first iceberg peak)"
            << std::endl;
  book.add_order(Order{6, Side::BUY, 101.00, 100});
  book.print_fills();
  book.print_market_depth_compact();

  // Test 4: Another buy to trigger refresh
  std::cout << "\nTest 4: BUY 80 @ $101.00 (partial fill of refreshed peak)"
            << std::endl;
  book.add_order(Order{7, Side::BUY, 101.00, 80});
  book.print_fills();
  book.print_order_status(5);
  book.print_market_depth_compact();

  // Test 5: Large order that walks through multiple refreshes
  std::cout << "\nTest 5: BUY 350 @ $101.00 (walk through multiple refreshes)"
            << std::endl;
  book.add_order(Order{8, Side::BUY, 101.00, 350});
  book.print_fills();
  book.print_order_status(5);
  book.print_market_depth(5);

  // Test 6: Iceberg on buy side
  std::cout << "\nTest 6: Iceberg BUY 1000 @ $99.50 (peak: 150)" << std::endl;
  book.add_order(Order{9, Side::BUY, 99.50, 1000, 150});
  book.print_market_depth_compact();

  // Test 7: Aggressive sell into iceberg
  std::cout << "\nTest 7: SELL 200 @ $99.00 (hit iceberg bid)" << std::endl;
  book.add_order(Order{10, Side::SELL, 99.00, 200});
  book.print_fills();
  book.print_order_status(9);

  // Final statistics
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "FINAL REPORT" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  book.print_match_stats();

  return 0;
}