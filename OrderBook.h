#pragma once
#include <list>
#include <map>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <optional>
#include "Order.h"
#include "ExecutionReport.h"
#include "ModifyResult.h"
#include <memory_resource>

class OrderBook {
    public:
        std::vector<ExecutionReport> addLimitOrder(Order&& order);
        std::vector<ExecutionReport> addMarketOrder(uint64_t id, Side side, uint32_t quantity);
        std::vector<ExecutionReport> addIOCOrder(uint64_t id, Side side, int64_t price, uint32_t quantity);
        std::vector<ExecutionReport> addFOKOrder(uint64_t id, Side side, int64_t price, uint32_t quantity);
        bool cancelOrder(uint64_t id);
        std::optional<int64_t> getBestBid() const;
        std::optional<int64_t> getBestAsk() const;
        ModifyResult modifyOrder(uint64_t id, int64_t newPrice, uint32_t newQuantity);
    private:
        struct OrderLocation {
            std::pmr::list<Order>::iterator it;
            Side side;
            int64_t price;
        };

        std::pmr::monotonic_buffer_resource mono{3200000};
        std::pmr::unsynchronized_pool_resource pool{&mono};

        std::pmr::map<int64_t, std::pmr::list<Order>, std::greater<int64_t>> bids{&pool};
        std::pmr::map<int64_t, std::pmr::list<Order>,  std::less<int64_t>> asks{&pool};
        std::unordered_map<uint64_t, OrderLocation> locations; 
        void matchAgainstBook(Side side, int64_t price, uint32_t& remaining, uint64_t incomingId, std::vector<ExecutionReport>& reports);
        uint64_t getAvailableQuantity(Side side, int64_t price);
};