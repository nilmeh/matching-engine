#pragma once
#include <cstdint>
#include <chrono>

enum class Side {
    BUY, SELL
};

enum class OrderType {
    LIMIT, MARKET, IOC, FOK
};

struct Order {
    uint64_t id;
    Side side;
    int64_t price;
    uint32_t quantity;
    std::chrono::steady_clock::time_point timestamp;
};