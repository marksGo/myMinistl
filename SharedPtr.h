#pragma once
#include <cstddef>
#include <utility>
#include <atomic>
#include <stdexcept>

// 控制块：同时管理强引用和弱引用计数
// 强引用归零 → 析构对象
// 强+弱引用均归零 → 释放控制块本身
struct ControlBlock {
    std::atomic<int> strongCount{1};
    std::atomic<int> weakCount{1};   // 额外的 1 代表所有 SharedPtr 的弱引用总量

    virtual void destroyObject() = 0;   // 析构被管理的对象
    virtual ~ControlBlock() = default;
};

template <typename T>
struct ControlBlockImpl : ControlBlock {
    T* ptr;
    explicit ControlBlockImpl(T* p) : ptr(p) {}
    void destroyObject() override { delete ptr; ptr = nullptr; }
};

// ── 前置声明 ─────────────────────────────────────────────────────────
template <typename T> class WeakPtr;

// ── SharedPtr ────────────────────────────────────────────────────────
template <typename T>
class SharedPtr {
    template <typename U> friend class SharedPtr;
    template <typename U> friend class WeakPtr;
public:
    // 构造
    SharedPtr() : ptr_(nullptr), ctrl_(nullptr) {}

    explicit SharedPtr(T* ptr)
        : ptr_(ptr)
        , ctrl_(ptr ? new ControlBlockImpl<T>(ptr) : nullptr) {}

    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) ctrl_->strongCount.fetch_add(1);
    }

    SharedPtr(SharedPtr&& other) noexcept : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_  = nullptr;
        other.ctrl_ = nullptr;
    }

    // 支持 SharedPtr<Base> = SharedPtr<Derived>
    template <typename U>
    SharedPtr(const SharedPtr<U>& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) ctrl_->strongCount.fetch_add(1);
    }

    ~SharedPtr() { release(); }

    SharedPtr& operator=(SharedPtr other) noexcept {
        swap(other);
        return *this;
    }

    // 访问
    T* get()        const { return ptr_; }
    T& operator*()  const { return *ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    long useCount() const {
        return ctrl_ ? ctrl_->strongCount.load() : 0;
    }

    void reset(T* ptr = nullptr) {
        SharedPtr tmp(ptr);
        swap(tmp);
    }

    void swap(SharedPtr& other) noexcept {
        std::swap(ptr_,  other.ptr_);
        std::swap(ctrl_, other.ctrl_);
    }

    bool operator==(const SharedPtr& o) const { return ptr_ == o.ptr_; }
    bool operator!=(const SharedPtr& o) const { return ptr_ != o.ptr_; }

private:
    // 由 WeakPtr::lock() 调用
    SharedPtr(T* ptr, ControlBlock* ctrl) : ptr_(ptr), ctrl_(ctrl) {
        if (ctrl_) ctrl_->strongCount.fetch_add(1);
    }

    void release() {
        if (!ctrl_) return;
        if (ctrl_->strongCount.fetch_sub(1) == 1) {
            // 强引用归零，析构对象
            ctrl_->destroyObject();
            // 减弱引用的那个额外 1
            if (ctrl_->weakCount.fetch_sub(1) == 1) {
                delete ctrl_;
            }
        }
        ptr_  = nullptr;
        ctrl_ = nullptr;
    }

    T*            ptr_;
    ControlBlock* ctrl_;
};

// 工厂函数，类似 std::make_shared（避免裸 new）
template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}

// ── WeakPtr ──────────────────────────────────────────────────────────
// 不增加强引用计数，通过 lock() 尝试获取 SharedPtr
// 解决 SharedPtr 循环引用导致的内存泄漏
template <typename T>
class WeakPtr {
    template <typename U> friend class SharedPtr;
public:
    WeakPtr() : ptr_(nullptr), ctrl_(nullptr) {}

    WeakPtr(const SharedPtr<T>& sp) : ptr_(sp.ptr_), ctrl_(sp.ctrl_) {
        if (ctrl_) ctrl_->weakCount.fetch_add(1);
    }

    WeakPtr(const WeakPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) ctrl_->weakCount.fetch_add(1);
    }

    WeakPtr(WeakPtr&& other) noexcept : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_  = nullptr;
        other.ctrl_ = nullptr;
    }

    ~WeakPtr() { release(); }

    WeakPtr& operator=(WeakPtr other) noexcept {
        std::swap(ptr_,  other.ptr_);
        std::swap(ctrl_, other.ctrl_);
        return *this;
    }

    bool expired() const {
        return !ctrl_ || ctrl_->strongCount.load() == 0;
    }

    // 尝试提升为 SharedPtr，失败返回空 SharedPtr
    SharedPtr<T> lock() const {
        if (!ctrl_) return SharedPtr<T>();
        // CAS：只有 strongCount > 0 时才能成功提升
        int expected = ctrl_->strongCount.load();
        while (expected > 0) {
            if (ctrl_->strongCount.compare_exchange_weak(expected, expected + 1)) {
                // 手动构造，不再走 SharedPtr(T*, ControlBlock*) 避免二次 +1
                SharedPtr<T> sp;
                sp.ptr_  = ptr_;
                sp.ctrl_ = ctrl_;
                return sp;
            }
        }
        return SharedPtr<T>();
    }

    long useCount() const {
        return ctrl_ ? ctrl_->strongCount.load() : 0;
    }

private:
    void release() {
        if (!ctrl_) return;
        if (ctrl_->weakCount.fetch_sub(1) == 1) {
            delete ctrl_;
        }
        ptr_  = nullptr;
        ctrl_ = nullptr;
    }

    T*            ptr_;
    ControlBlock* ctrl_;
};
