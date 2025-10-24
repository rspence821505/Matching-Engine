#include "order_book.hpp"
#include <iostream>
#include <string>

int main() {
  std::cout << "=== Stop Orders ===" << std::endl << std::endl;

  OrderBook book;

  // Test 1: Build initial book
  std::cout << "Test 1: Building initial book around $100" << std::endl;
  book.add_order(Order{1, Side::BUY, 100.00, 200});
  book.add_order(Order{2, Side::BUY, 99.50, 300});
  book.add_order(Order{3, Side::SELL, 101.00, 150});
  book.add_order(Order{4, Side::SELL, 100.50, 200});
  book.print_market_depth(5);

  // Test 2: Place stop-loss orders (sell stops below market)
  std::cout << "\nTest 2: Placing stop-loss SELL orders" << std::endl;
  book.add_order(Order{5, Side::SELL, 98.00, 100, true}); // Stop-market @ $98
  book.add_order(Order{6, Side::SELL, 97.00, 95.00, 150,
                       TimeInForce::GTC}); // Stop-limit @ $97 → $95
  book.print_pending_stops();

  // Test 3: Place stop-buy orders (buy stops above market)
  std::cout << "\nTest 3: Placing stop-buy BUY orders" << std::endl;
  book.add_order(Order{7, Side::BUY, 102.00, 120, true}); // Stop-market @ $102
  book.add_order(Order{8, Side::BUY, 103.00, 105.00, 100,
                       TimeInForce::GTC}); // Stop-limit @ $103 → $105
  book.print_pending_stops();

  // Test 4: Price rises - trigger stop-buy
  std::cout << "\nTest 4: Aggressive SELL pushes price up (trigger stop-buy)"
            << std::endl;
  book.add_order(Order{9, Side::SELL, 102.50, 250}); // This will trade at $102+
  book.print_fills();
  book.print_pending_stops();
  book.print_market_depth_compact();

  // Test 5: Price falls - trigger stop-loss
  std::cout << "\nTest 5: Aggressive BUY pushes price down (trigger stop-loss)"
            << std::endl;
  book.add_order(Order{10, Side::BUY, 97.50, 200}); // This will trade at $98-
  book.print_fills();
  book.print_pending_stops();
  book.print_market_depth_compact();

  // Test 6: Cascading stops (stop spiral)
  std::cout << "\nTest 6: Test cascading stop triggers" << std::endl;
  book.add_order(Order{11, Side::SELL, 96.50, 100, true}); // Stop @ $96.50
  book.add_order(Order{12, Side::SELL, 96.00, 100, true}); // Stop @ $96.00
  book.add_order(Order{13, Side::SELL, 95.50, 100, true}); // Stop @ $95.50

  std::cout << "\nBefore cascade:" << std::endl;
  book.print_pending_stops();

  // Trigger cascade
  book.add_order(Order{14, Side::BUY, 96.40, 50});

  std::cout << "\nAfter cascade:" << std::endl;
  book.print_fills();
  book.print_pending_stops();

  // Final statistics
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "FINAL REPORT" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  book.print_match_stats();
  std::cout << "Pending stops: " << book.pending_stop_count() << std::endl;

  return 0;
}
