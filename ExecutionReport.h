#pragma once
#include <cstdint>
#include <chrono>
#include "Order.h"

struct ExecutionReport {
    uint64_t bid_id;
    uint64_t ask_id;
    int64_t price;
    uint32_t quantity;
    std::chrono::steady_clock::time_point timestamp;
    Side aggressor;

};