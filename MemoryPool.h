#pragma once
#include <cstddef>
#include <cassert>
#include <vector>
#include <mutex>

// Slab 风格内存池：每个池管理固定大小的块
// 核心思路：预先申请大块内存（slab），切成等大的 chunk，用空闲链表管理
// 分配/释放均为 O(1)，避免频繁调用 malloc/free 的系统开销
template <size_t BlockSize, size_t BlocksPerSlab = 64>
class MemoryPool {
    static_assert(BlockSize >= sizeof(void*), "BlockSize too small");
public:
    MemoryPool() : freeList_(nullptr), allocated_(0) {}

    ~MemoryPool() {
        for (auto* slab : slabs_) {
            ::operator delete(slab);
        }
    }

    // 分配一个 BlockSize 大小的块，O(1)
    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!freeList_) growSlab();
        void* ptr = freeList_;
        freeList_ = *reinterpret_cast<void**>(freeList_);
        ++allocated_;
        return ptr;
    }

    // 归还一个块，O(1)
    void deallocate(void* ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mutex_);
        // 把 ptr 头部当指针存下一个空闲块地址
        *reinterpret_cast<void**>(ptr) = freeList_;
        freeList_ = ptr;
        --allocated_;
    }

    size_t allocated() const { return allocated_; }

    // 禁止拷贝
    MemoryPool(const MemoryPool&)            = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

private:
    void growSlab() {
        // 申请一大块内存，切成 BlocksPerSlab 个 BlockSize 的块
        void* slab = ::operator new(BlockSize * BlocksPerSlab);
        slabs_.push_back(slab);

        // 把每个块的头部用作指针，串成空闲链表
        char* p = static_cast<char*>(slab);
        for (size_t i = 0; i < BlocksPerSlab - 1; ++i) {
            *reinterpret_cast<void**>(p) = p + BlockSize;
            p += BlockSize;
        }
        *reinterpret_cast<void**>(p) = freeList_;  // 尾部接上原链表
        freeList_ = slab;
    }

    void*              freeList_;    // 空闲块链表头
    std::vector<void*> slabs_;       // 所有已申请的大块（用于析构释放）
    size_t             allocated_;   // 当前已分配块数（调试用）
    std::mutex         mutex_;
};


// ── 通用对象池（在 MemoryPool 基础上封装构造/析构） ──────────────────
template <typename T, size_t BlocksPerSlab = 64>
class ObjectPool {
public:
    template <typename... Args>
    T* construct(Args&&... args) {
        void* mem = pool_.allocate();
        return new(mem) T(std::forward<Args>(args)...);
    }

    void destroy(T* obj) {
        if (!obj) return;
        obj->~T();
        pool_.deallocate(obj);
    }

    size_t allocated() const { return pool_.allocated(); }

private:
    MemoryPool<sizeof(T), BlocksPerSlab> pool_;
};
