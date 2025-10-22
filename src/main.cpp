#include "order_book.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Order Cancellation & Amendment ===" << std::endl
            << std::endl;

  OrderBook book;

  // Test 1: Build initial book
  std::cout << "Test 1: Building initial book" << std::endl;
  book.add_order(Order{1, Side::BUY, 100.00, 100});
  book.add_order(Order{2, Side::BUY, 99.75, 200});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.75, 100});
  book.print_book_summary();

  // Test 2: Cancel an order
  std::cout << "\nTest 2: Cancelling order #2" << std::endl;
  book.cancel_order(2);
  book.print_order_status(2);
  book.print_book_summary();

  // Test 3: Try to cancel non-existent order
  std::cout << "\nTest 3: Try to cancel non-existent order #999" << std::endl;
  book.cancel_order(999);

  // Test 4: Amend an order
  std::cout << "\nTest 4: Amending order #3 (change price)" << std::endl;
  book.print_order_status(3);
  book.amend_order(3, 100.50, std::nullopt); // Lower ask price
  book.print_order_status(3);
  book.print_book_summary();

  // Test 5: Add aggressive order that matches
  std::cout << "\nTest 5: Add aggressive buy that matches amended ask"
            << std::endl;
  book.add_order(Order{5, Side::BUY, 101.00, 120});
  book.print_fills();
  book.print_book_summary();

  // Test 6: Try to cancel filled order
  std::cout << "\nTest 6: Try to cancel filled order #3" << std::endl;
  book.cancel_order(3);

  // Final statistics
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "FINAL REPORT" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  book.print_match_stats();

  return 0;
}