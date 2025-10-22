#include "order_book.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Market Orders ===" << std::endl << std::endl;

  OrderBook book;

  // Test 1: Build initial book with limit orders
  std::cout << "Test 1: Building initial book (limit orders)" << std::endl;
  book.add_order(Order{1, Side::BUY, 100.00, 100});
  book.add_order(Order{2, Side::BUY, 99.75, 200});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.75, 100});
  book.print_book_summary();

  // Test 2: Market buy order (should match best ask)
  std::cout << "\nTest 2: Market BUY order (50 shares)" << std::endl;
  book.add_order(Order{5, Side::BUY, OrderType::MARKET, 50});
  book.print_fills();
  book.print_book_summary();

  // Test 3: Large market buy (consumes multiple levels)
  std::cout << "\nTest 3: Large market BUY order (200 shares)" << std::endl;
  book.add_order(Order{6, Side::BUY, OrderType::MARKET, 200});
  book.print_fills();
  book.print_book_summary();

  // Test 4: Market sell order
  std::cout << "\nTest 4: Market SELL order (80 shares)" << std::endl;
  book.add_order(Order{7, Side::SELL, OrderType::MARKET, 80});
  book.print_fills();
  book.print_book_summary();

  // Test 5: Market order with insufficient liquidity
  std::cout << "\nTest 5: Market BUY with insufficient liquidity (1000 shares)"
            << std::endl;
  book.add_order(Order{8, Side::BUY, OrderType::MARKET, 1000});
  book.print_fills();
  book.print_book_summary();

  // Final statistics
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "FINAL REPORT" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  book.print_match_stats();

  return 0;
}
