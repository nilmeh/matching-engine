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
            std::list<Order>::iterator it;
            Side side;
            int64_t price;
        };

        std::map<int64_t, std::list<Order>, std::greater<int64_t>> bids;
        std::map<int64_t, std::list<Order>,  std::less<int64_t>> asks;
        std::unordered_map<uint64_t, OrderLocation> locations; 
        void matchAgainstBook(Side side, int64_t price, uint32_t& remaining, uint64_t incomingId, std::vector<ExecutionReport>& reports);
        uint32_t getAvailableQuantity(Side side, int64_t price);
};