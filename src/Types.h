#include <cstdint>

enum class Side : uint8_t {Buy = 0, Sell = 1};

struct Order {
    uint64_t id;
    Side side;
    int64_t price;
    uint32_t qty;
    
    Order* next = nullptr;
    Order* prev = nullptr; 
};