#include "order_book.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Time-in-Force Rules ===" << std::endl << std::endl;

  OrderBook book;

  // Test 1: Build initial book
  std::cout << "Test 1: Building initial book (GTC limit orders)" << std::endl;
  book.add_order(Order{1, Side::BUY, 100.00, 100});
  book.add_order(Order{2, Side::BUY, 99.75, 200});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.75, 100});
  book.print_book_summary();

  // Test 2: IOC order - partial fill
  std::cout << "\nTest 2: IOC BUY 120 @ $101.00 (should partially fill)"
            << std::endl;
  book.add_order(Order{5, Side::BUY, 101.00, 120, TimeInForce::IOC});
  book.print_fills();
  book.print_book_summary();

  // Test 3: IOC order - no fill
  std::cout << "\nTest 3: IOC BUY 50 @ $100.00 (should cancel - no match)"
            << std::endl;
  book.add_order(Order{6, Side::BUY, 100.00, 50, TimeInForce::IOC});
  book.print_fills();
  book.print_book_summary();

  // Test 4: FOK order - can fill completely
  std::cout << "\nTest 4: FOK SELL 50 @ $99.00 (should fill completely)"
            << std::endl;
  book.add_order(Order{7, Side::SELL, 99.00, 50, TimeInForce::FOK});
  book.print_fills();
  book.print_book_summary();

  // Test 5: FOK order - cannot fill completely
  std::cout << "\nTest 5: FOK SELL 500 @ $99.00 (should cancel - insufficient "
               "liquidity)"
            << std::endl;
  book.add_order(Order{8, Side::SELL, 99.00, 500, TimeInForce::FOK});
  book.print_fills();
  book.print_book_summary();

  // Test 6: GTC order - rests in book
  std::cout << "\nTest 6: GTC BUY 200 @ $99.50 (should rest in book)"
            << std::endl;
  book.add_order(Order{9, Side::BUY, 99.50, 200, TimeInForce::GTC});
  book.print_book_summary();

  // Test 7: DAY order - rests in book (same as GTC for now)
  std::cout << "\nTest 7: DAY SELL 100 @ $101.50 (should rest in book)"
            << std::endl;
  book.add_order(Order{10, Side::SELL, 101.50, 100, TimeInForce::DAY});
  book.print_book_summary();

  // Final statistics
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "FINAL REPORT" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  book.print_match_stats();

  return 0;
}