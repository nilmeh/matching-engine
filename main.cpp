#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include "Order.h"
#include "OrderBook.h"

int main() {

    // ─── modifyOrder tests ────────────────────────────────────────────────

    {
        OrderBook book;

        // Test 1: price change — should lose time priority (cancel + reinsert)
        // Add two resting bids at 10000, then modify the first one's price
        // After modify, order 1 should be at the BACK of the new price level's queue
        book.addLimitOrder(Order{1, Side::BUY, 10000, 100, std::chrono::steady_clock::now()});
        book.addLimitOrder(Order{2, Side::BUY, 10000, 50,  std::chrono::steady_clock::now()});

        ModifyResult r1 = book.modifyOrder(1, 10000, 150); // price same, qty UP → reinsert
        std::cout << "[Test 1 - qty increase loses priority]\n";
        std::cout << "success=" << r1.success << " fills=" << r1.reports.size() << "\n";

        // Now send a sell that sweeps 50 qty — should hit order 2 first (it has priority)
        auto fills = book.addLimitOrder(Order{3, Side::SELL, 9000, 50, std::chrono::steady_clock::now()});
        for (const auto& f : fills)
            std::cout << "FILL: bid=" << f.bid_id << " ask=" << f.ask_id
                      << " price=" << f.price << " qty=" << f.quantity << "\n";
        // Expected: bid=2, ask=3 — order 2 had priority, order 1 went to back
    }

            // FOK kill case - not enough liquidity
        OrderBook book1;
        book1.addLimitOrder(Order{1, Side::SELL, 10000, 50, std::chrono::steady_clock::now()});
        auto fills1 = book1.addFOKOrder(2, Side::BUY, 10000, 100); // wants 100, only 50 available
        std::cout << "FOK kill fills: " << fills1.size() << "\n"; // should be 0

        // FOK fill case - enough liquidity  
        OrderBook book2;
        book2.addLimitOrder(Order{1, Side::SELL, 10000, 100, std::chrono::steady_clock::now()});
        auto fills2 = book2.addFOKOrder(2, Side::BUY, 10000, 50); // wants 50, 100 available
        std::cout << "FOK fill fills: " << fills2.size() << "\n"; // should be 1

    {
        OrderBook book;

        // Test 2: quantity decrease — should preserve time priority
        book.addLimitOrder(Order{1, Side::BUY, 10000, 100, std::chrono::steady_clock::now()});
        book.addLimitOrder(Order{2, Side::BUY, 10000, 50,  std::chrono::steady_clock::now()});

        ModifyResult r2 = book.modifyOrder(1, 10000, 60); // qty DOWN → in-place
        std::cout << "\n[Test 2 - qty decrease preserves priority]\n";
        std::cout << "success=" << r2.success << " fills=" << r2.reports.size() << "\n";

        // Send a sell for 60 — should hit order 1 first (still has priority)
        auto fills = book.addLimitOrder(Order{3, Side::SELL, 9000, 60, std::chrono::steady_clock::now()});
        for (const auto& f : fills)
            std::cout << "FILL: bid=" << f.bid_id << " ask=" << f.ask_id
                      << " price=" << f.price << " qty=" << f.quantity << "\n";
        // Expected: bid=1, ask=3 — order 1 still at front of queue
    }

    {
        OrderBook book;

        // Test 3: no-op — same price and quantity
        book.addLimitOrder(Order{1, Side::BUY, 10000, 100, std::chrono::steady_clock::now()});

        ModifyResult r3 = book.modifyOrder(1, 10000, 100);
        std::cout << "\n[Test 3 - no-op]\n";
        std::cout << "success=" << r3.success << "\n";
        // Expected: success=0 (false)
    }

    {
        OrderBook book;

        // Test 4: modify nonexistent order
        ModifyResult r4 = book.modifyOrder(999, 10000, 100);
        std::cout << "\n[Test 4 - nonexistent order]\n";
        std::cout << "success=" << r4.success << "\n";
        // Expected: success=0 (false)
    }

    std::cout << "\n─── Benchmark ───────────────────────────────────────────\n";

    // ─── Benchmark ────────────────────────────────────────────────────────

    OrderBook book;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t>  price_dist(9900, 10100);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int>      side_dist(0, 1);

    const int N = 1000000;
    std::vector<Order> orders;
    orders.reserve(N);

    for (int i = 0; i < N; i++) {
        Side side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
        orders.push_back(Order{
            static_cast<uint64_t>(i + 1),
            side,
            price_dist(rng),
            qty_dist(rng),
            std::chrono::steady_clock::now()
        });
    }

    size_t total_fills = 0;
    std::vector<double> latencies;
latencies.reserve(N);

for (int i = 0; i < N; i++) {
    auto start = std::chrono::steady_clock::now();
    auto reports = book.addLimitOrder(std::move(orders[i]));
    auto end = std::chrono::steady_clock::now();
    
    total_fills += reports.size();
    latencies.push_back(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
    );
}

std::sort(latencies.begin(), latencies.end());

std::cout << "p50:  " << latencies[N * 0.50] << " ns\n";
std::cout << "p99:  " << latencies[N * 0.99] << " ns\n";
std::cout << "p99.9:" << latencies[N * 0.999] << " ns\n";
std::cout << "max:  " << latencies[N - 1] << " ns\n";

    // --- throughput run: time the whole loop, no per-call clock reads ---
    OrderBook book_tp;
    std::vector<Order> orders_tp;
    orders_tp.reserve(N);
    for (int i = 0; i < N; i++) {
        Side side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
        orders_tp.push_back(Order{
            static_cast<uint64_t>(i + 1), side, price_dist(rng), qty_dist(rng),
            std::chrono::steady_clock::now()
        });
    }

    size_t fills_tp = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; i++) {
        fills_tp += book_tp.addLimitOrder(std::move(orders_tp[i])).size();
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "throughput: " << N / secs << " orders/sec (fills: " << fills_tp << ")\n";

    return 0;
}