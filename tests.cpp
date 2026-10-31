#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
#include "OrderBook.h"

static Order mk(uint64_t id, Side side, int64_t price, uint32_t qty) {
    return Order{id, side, price, qty, std::chrono::steady_clock::now()};
}

struct Case { const char* name; void (*fn)(); };
static std::vector<Case>& tests() { static std::vector<Case> t; return t; }
#define TEST(name) \
    static void name(); \
    static const bool name##_registered = (tests().push_back({#name, name}), true); \
    static void name()

TEST(limit_order_rests_when_no_cross) {
    OrderBook b;
    assert(b.addLimitOrder(mk(1, Side::BUY, 100, 10)).empty());
    assert(b.addLimitOrder(mk(2, Side::SELL, 101, 10)).empty());
    assert(b.getBestBid() == 100 && b.getBestAsk() == 101);
}

TEST(full_match_executes_at_resting_price) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 10));
    auto r = b.addLimitOrder(mk(2, Side::BUY, 105, 10));
    assert(r.size() == 1);
    assert(r[0].price == 100 && r[0].quantity == 10);
    assert(r[0].bid_id == 2 && r[0].ask_id == 1 && r[0].aggressor == Side::BUY);
    assert(!b.getBestAsk() && !b.getBestBid());
}

TEST(price_time_priority_within_level) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::BUY, 100, 10));
    b.addLimitOrder(mk(2, Side::BUY, 100, 10));
    auto r = b.addLimitOrder(mk(3, Side::SELL, 100, 10));
    assert(r.size() == 1 && r[0].bid_id == 1);
}

TEST(better_price_matches_first) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 102, 5));
    b.addLimitOrder(mk(2, Side::SELL, 101, 5));
    auto r = b.addLimitOrder(mk(3, Side::BUY, 102, 8));
    assert(r.size() == 2);
    assert(r[0].ask_id == 2 && r[0].price == 101 && r[0].quantity == 5);
    assert(r[1].ask_id == 1 && r[1].price == 102 && r[1].quantity == 3);
}

// Regression: a partially filled incoming order must rest with its *remaining* quantity.
TEST(partial_fill_rests_remaining_quantity) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 30));
    auto r = b.addLimitOrder(mk(2, Side::BUY, 100, 100));
    assert(r.size() == 1 && r[0].quantity == 30);
    assert(b.getBestBid() == 100);
    auto r2 = b.addLimitOrder(mk(3, Side::SELL, 100, 100));
    assert(r2.size() == 1 && r2[0].quantity == 70);
}

TEST(sweeps_multiple_levels_and_stops_at_limit) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 5));
    b.addLimitOrder(mk(2, Side::SELL, 101, 5));
    b.addLimitOrder(mk(3, Side::SELL, 103, 5));
    auto r = b.addLimitOrder(mk(4, Side::BUY, 102, 100));
    assert(r.size() == 2);
    assert(b.getBestAsk() == 103 && b.getBestBid() == 102);
}

TEST(cancel_removes_order_and_empty_level) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::BUY, 100, 10));
    b.addLimitOrder(mk(2, Side::BUY, 99, 10));
    assert(b.cancelOrder(1));
    assert(b.getBestBid() == 99);
    assert(!b.cancelOrder(1));
    assert(!b.cancelOrder(999));
    assert(b.addLimitOrder(mk(3, Side::SELL, 99, 10)).size() == 1);
}

TEST(cancel_after_full_fill_fails) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 10));
    b.addLimitOrder(mk(2, Side::BUY, 100, 10));
    assert(!b.cancelOrder(1));
}

TEST(market_order_sweeps_and_never_rests) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 5));
    b.addLimitOrder(mk(2, Side::SELL, 200, 5));
    auto r = b.addMarketOrder(3, Side::BUY, 8);
    assert(r.size() == 2 && r[0].quantity == 5 && r[1].quantity == 3);
    auto r2 = b.addMarketOrder(4, Side::BUY, 100);   // only 2 left; remainder is dropped
    assert(r2.size() == 1 && r2[0].quantity == 2);
    assert(!b.getBestBid());
}

TEST(ioc_fills_what_it_can_and_discards_rest) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 5));
    auto r = b.addIOCOrder(2, Side::BUY, 100, 20);
    assert(r.size() == 1 && r[0].quantity == 5);
    assert(!b.getBestBid());
}

TEST(fok_kills_without_liquidity_and_leaves_book_intact) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 50));
    assert(b.addFOKOrder(2, Side::BUY, 100, 100).empty());
    assert(b.getBestAsk() == 100);
    assert(b.addLimitOrder(mk(3, Side::BUY, 100, 50)).size() == 1);   // order 1 still intact at 50
}

TEST(fok_fills_across_levels_when_liquidity_is_enough) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::SELL, 100, 50));
    b.addLimitOrder(mk(2, Side::SELL, 101, 50));
    auto r = b.addFOKOrder(3, Side::BUY, 101, 100);
    assert(r.size() == 2);
    assert(!b.getBestAsk());
}

TEST(modify_quantity_increase_loses_priority) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::BUY, 100, 100));
    b.addLimitOrder(mk(2, Side::BUY, 100, 50));
    auto m = b.modifyOrder(1, 100, 150);
    assert(m.success && m.reports.empty());
    auto r = b.addLimitOrder(mk(3, Side::SELL, 90, 50));
    assert(r.size() == 1 && r[0].bid_id == 2);
}

TEST(modify_quantity_decrease_keeps_priority) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::BUY, 100, 100));
    b.addLimitOrder(mk(2, Side::BUY, 100, 50));
    assert(b.modifyOrder(1, 100, 60).success);
    auto r = b.addLimitOrder(mk(3, Side::SELL, 90, 100));
    assert(r.size() == 2 && r[0].bid_id == 1 && r[0].quantity == 60 && r[1].bid_id == 2);
}

TEST(modify_price_moves_order_and_can_cross) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::BUY, 99, 10));
    b.addLimitOrder(mk(2, Side::SELL, 101, 10));
    auto m = b.modifyOrder(1, 101, 10);
    assert(m.success && m.reports.size() == 1 && m.reports[0].ask_id == 2);
    assert(!b.getBestBid() && !b.getBestAsk());
}

TEST(modify_noop_unknown_and_zero_quantity_fail) {
    OrderBook b;
    b.addLimitOrder(mk(1, Side::BUY, 100, 100));
    assert(!b.modifyOrder(1, 100, 100).success);
    assert(!b.modifyOrder(999, 100, 100).success);
    assert(!b.modifyOrder(1, 100, 0).success);
    assert(b.getBestBid() == 100);
}

TEST(rejects_zero_quantity_and_duplicate_ids) {
    OrderBook b;
    assert(b.addLimitOrder(mk(1, Side::BUY, 100, 0)).empty());
    assert(!b.getBestBid());
    b.addLimitOrder(mk(2, Side::BUY, 100, 10));
    b.addLimitOrder(mk(2, Side::BUY, 105, 10));   // duplicate id: ignored
    assert(b.getBestBid() == 100);
    assert(b.cancelOrder(2) && !b.getBestBid());
}

int main() {
    for (const auto& t : tests()) {
        t.fn();
        std::cout << "PASS " << t.name << "\n";
    }
    std::cout << tests().size() << " tests passed\n";
}
