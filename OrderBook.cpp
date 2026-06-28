#include "OrderBook.h"
#include <chrono>

std::vector<ExecutionReport> OrderBook::addLimitOrder(Order&& order) {
    std::vector<ExecutionReport> reports;
    uint32_t remaining = order.quantity;
    matchAgainstBook(order.side, order.price, remaining, order.id, reports);
    if (remaining > 0) {
        if (order.side == Side::BUY) {
            bids[order.price].push_back(order);
            locations[order.id] = OrderLocation{std::prev(bids[order.price].end()), order.side, order.price};
        }
        else {
            asks[order.price].push_back(order);
            locations[order.id] = OrderLocation{std::prev(asks[order.price].end()), order.side, order.price};
        }
    }
    return reports;
}

std::vector<ExecutionReport> OrderBook::addMarketOrder(uint64_t id, Side side, uint32_t quantity) {
    uint32_t remaining = quantity;
    int64_t price = (side == Side::BUY) ? INT64_MAX : INT64_MIN;
    std::vector<ExecutionReport> reports;
    matchAgainstBook(side, price, remaining, id, reports);
    return reports;
}

std::vector<ExecutionReport> OrderBook::addIOCOrder(uint64_t id, Side side, int64_t price, uint32_t quantity) {
    uint32_t remaining = quantity;
    std::vector<ExecutionReport> reports;
    matchAgainstBook(side, price, remaining, id, reports);
    return reports;
}

std::vector<ExecutionReport> OrderBook::addFOKOrder(uint64_t id, Side side, int64_t price, uint32_t quantity) {
    uint32_t remaining = quantity;
    std::vector<ExecutionReport> reports;
    if (remaining > getAvailableQuantity(side, price)) {
        return {};
    } 
    matchAgainstBook(side, price, remaining, id, reports);
    return reports;
}

bool OrderBook::cancelOrder(uint64_t id) {
    auto found = locations.find(id);
    if (found == locations.end()) return false;
    OrderLocation location = found->second;
    if (location.side == Side::BUY) {
        bids[location.price].erase(location.it);
        if (bids[location.price].empty()) bids.erase(location.price);
    } else {
        asks[location.price].erase(location.it);
        if (asks[location.price].empty()) asks.erase(location.price);
    }
    locations.erase(id);
    return true;
}

std::optional<int64_t> OrderBook::getBestBid() const {
    if (bids.empty()) {
        return std::nullopt;
    }
    return bids.begin() -> first;
}

std::optional<int64_t> OrderBook::getBestAsk() const {
    if (asks.empty()) {
        return std::nullopt;
    }
    return asks.begin() -> first;
}

ModifyResult OrderBook::modifyOrder(uint64_t id, int64_t newPrice, uint32_t newQuantity) {
    ModifyResult result;
    auto locIt = locations.find(id);
    if (locIt == locations.end()) {
        result.success = false;
        return result;
    }
    OrderLocation loc = locIt -> second;
    Order& order = *loc.it;
    
    if (newPrice != order.price || newQuantity > order.quantity) {
        Side oldSide = order.side;
        cancelOrder(id);
        result.reports = addLimitOrder(Order{id, oldSide, newPrice, newQuantity, std::chrono::steady_clock::now()});
        result.success = true;
        return result;
    } else if (newQuantity < order.quantity) {
        order.quantity = newQuantity;
        result.success = true;
    } else {
        result.success = false;
    }
    return result;
}

void OrderBook::matchAgainstBook(Side side, int64_t price, uint32_t& remaining, uint64_t incomingId, std::vector<ExecutionReport>& reports) {
    if (side == Side::BUY) {
        auto lowest = asks.begin();
        while (!asks.empty() && lowest != asks.end() && lowest -> first <= price) {
            uint32_t fill_qty = std::min(lowest -> second.front().quantity, remaining);
            ExecutionReport report {
                incomingId,
                lowest -> second.front().id,
                lowest -> first,
                fill_qty,
                std::chrono::steady_clock::now(),
                Side::BUY
            };
            reports.push_back(report);
            remaining -= fill_qty;
            lowest -> second.front().quantity -= fill_qty;
            if (lowest -> second.front().quantity == 0){
                locations.erase(lowest -> second.front().id);
                lowest -> second.pop_front();
                if (lowest -> second.empty()) {
                    lowest = asks.erase(lowest);
                }
                if (remaining == 0){
                    break;
                }
            }
            else {
                if (remaining == 0){
                    break;
                }
            }
        }
    }
    else {
        auto highest = bids.begin();
        while (!bids.empty() && highest != bids.end() && highest -> first >= price) {
            uint32_t fill_qty = std::min(highest -> second.front().quantity, remaining);
            ExecutionReport report {
                highest -> second.front().id,
                incomingId,
                highest -> first,
                fill_qty,
                std::chrono::steady_clock::now(),
                Side::SELL
            };
            reports.push_back(report);
            remaining -= fill_qty;
            highest -> second.front().quantity -= fill_qty;
            if (highest -> second.front().quantity == 0){
                locations.erase(highest -> second.front().id);
                highest -> second.pop_front();
                if (highest -> second.empty()) {
                    highest = bids.erase(highest);
                }
                if (remaining == 0){
                    break;
                }
            }
            else {
                if (remaining == 0){
                    break;
                }
            }
        }
    }
}

uint32_t OrderBook::getAvailableQuantity(Side side, int64_t price) {
    uint32_t total_qty = 0;
    if (side == Side::BUY) {
        for (auto it = asks.begin(); it != asks.end(); ++it) {
            if(it -> first > price) {
                break;
            }
            for (const auto& order : it -> second) {
                total_qty += order.quantity;
            }
        }
    } else {
        for (auto it = bids.begin(); it != bids.end(); ++it) {
            if(it -> first < price) {
                break;
            }
            for (const auto& order : it -> second) {
                total_qty += order.quantity;
            }
        }
    }

    return total_qty;
}