#pragma once
#include "MemoryPool.h"
#include <cstddef>
#include <limits>

// 符合 C++11 Allocator 接口的内存池分配器
// 可直接接入 std::vector, std::list, std::unordered_map 等标准容器：
//   std::vector<int, PoolAllocator<int>> v;
template <typename T>
class PoolAllocator {
public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind { using other = PoolAllocator<U>; };

    PoolAllocator()  = default;
    ~PoolAllocator() = default;

    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) {}

    // 分配 n 个 T 的内存
    // 小对象（≤ 256B）走内存池，大对象直接 malloc
    T* allocate(size_type n) {
        size_t bytes = n * sizeof(T);
        if (n == 1 && sizeof(T) <= 256) {
            return static_cast<T*>(pool().allocate());
        }
        return static_cast<T*>(::operator new(bytes));
    }

    void deallocate(T* ptr, size_type n) {
        if (n == 1 && sizeof(T) <= 256) {
            pool().deallocate(ptr);
        } else {
            ::operator delete(ptr);
        }
    }

    size_type max_size() const {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    template <typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        new(ptr) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* ptr) { ptr->~U(); }

    bool operator==(const PoolAllocator&) const { return true; }
    bool operator!=(const PoolAllocator&) const { return false; }

private:
    // 每个 T 类型共享一个静态内存池（进程级别）
    static MemoryPool<sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T)>&
    pool() {
        static MemoryPool<sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T)> p;
        return p;
    }
};

// void 特化（STL 要求）
template <>
class PoolAllocator<void> {
public:
    using value_type = void;
    using pointer    = void*;
    template <typename U>
    struct rebind { using other = PoolAllocator<U>; };
};
