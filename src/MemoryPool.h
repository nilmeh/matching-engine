#pragma once
#include <cstdint>
#include <vector>

template <typename T, size_t N>
class MemoryPool {
    public:
    MemoryPool() {
        for (size_t i = 0; i < N; i++) {
            free_indices[i] = i
        }
        next_free = 0
    }
    
    T* allocate() {
        if(next_free >= N) return nullptr;
        return &pool[free_indices[next_free++]];
    }
    
    void deallocate(T* ptr) {
        size_t index = ptr - pool.data()
        free_indices[--next_free] = index;
    }
    
    private:
        std::vector<T> pool = std::vector<T>(N);
        size_t free_indices[N];
        size_t next_free;
};