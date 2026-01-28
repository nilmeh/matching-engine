#pragma once
#include "Types.h"
#include <cstdint>
#include <array>
#include <unordered_map>

constexpr int MAX_PRICE = 100000;

struct PriceLevel {
    Order* head = nullptr;
    Order* tail = nullptr;

    void addOrder(Order* o) {
        o -> next = nullptr;
        o -> prev = tail;
        if (tail) tail-> next = o;
        tail = o;
        if(!head) head = o;
    }

    void removeOrder(Order* o) {
        if (o -> prev) o->prev->next = o -> next;
        else head = o -> next;

        if (o-> next) o->next->prev = o->prev;
        else tail = o -> prev;

        o-> next = o-> prev = nullptr;

    }
    bool empty() const{ return head == nullptr; }
};
