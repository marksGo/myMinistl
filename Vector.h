#pragma once
#include <cstddef>
#include <stdexcept>
#include <initializer_list>
#include <algorithm>
#include <iterator>

// 手写 vector：动态数组
// 核心点：
//   - 2倍扩容策略（均摊 O(1) push_back）
//   - 连续内存保证随机访问 O(1)
//   - 扩容时原迭代器全部失效（重要！面试必问）
template <typename T>
class Vector {
public:
    // ── 迭代器 ─────────────────────────────────────────────────────
    using iterator       = T*;
    using const_iterator = const T*;
    using size_type      = std::size_t;

    iterator       begin()        { return data_; }
    iterator       end()          { return data_ + size_; }
    const_iterator begin()  const { return data_; }
    const_iterator end()    const { return data_ + size_; }
    const_iterator cbegin() const { return data_; }
    const_iterator cend()   const { return data_ + size_; }

    // ── 构造 / 析构 ─────────────────────────────────────────────────
    Vector() : data_(nullptr), size_(0), capacity_(0) {}

    explicit Vector(size_type n, const T& val = T())
        : data_(nullptr), size_(0), capacity_(0) {
        resize(n, val);
    }

    Vector(std::initializer_list<T> il)
        : data_(nullptr), size_(0), capacity_(0) {
        reserve(il.size());
        for (const auto& v : il) push_back(v);
    }

    Vector(const Vector& other) : data_(nullptr), size_(0), capacity_(0) {
        reserve(other.size_);
        for (size_type i = 0; i < other.size_; ++i)
            new(data_ + i) T(other.data_[i]);
        size_ = other.size_;
    }

    Vector(Vector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_     = nullptr;
        other.size_     = 0;
        other.capacity_ = 0;
    }

    ~Vector() { clear(); ::operator delete(data_); }

    Vector& operator=(Vector other) {   // copy-and-swap
        swap(other);
        return *this;
    }

    // ── 容量 ────────────────────────────────────────────────────────
    size_type size()     const { return size_; }
    size_type capacity() const { return capacity_; }
    bool      empty()    const { return size_ == 0; }

    // 预分配空间（不改变 size）
    void reserve(size_type newCap) {
        if (newCap <= capacity_) return;
        T* newData = static_cast<T*>(::operator new(newCap * sizeof(T)));
        // 移动原有元素到新内存
        for (size_type i = 0; i < size_; ++i) {
            new(newData + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        ::operator delete(data_);
        data_     = newData;
        capacity_ = newCap;
    }

    void resize(size_type n, const T& val = T()) {
        if (n > capacity_) reserve(n * 2);
        if (n > size_) {
            for (size_type i = size_; i < n; ++i) new(data_ + i) T(val);
        } else {
            for (size_type i = n; i < size_; ++i) data_[i].~T();
        }
        size_ = n;
    }

    // ── 元素访问 ─────────────────────────────────────────────────────
    T&       operator[](size_type i)       { return data_[i]; }
    const T& operator[](size_type i) const { return data_[i]; }

    T& at(size_type i) {
        if (i >= size_) throw std::out_of_range("Vector::at out of range");
        return data_[i];
    }

    T&       front()       { return data_[0]; }
    const T& front() const { return data_[0]; }
    T&       back()        { return data_[size_ - 1]; }
    const T& back()  const { return data_[size_ - 1]; }
    T*       data()        { return data_; }

    // ── 修改 ─────────────────────────────────────────────────────────
    void push_back(const T& val) {
        if (size_ == capacity_) grow();
        new(data_ + size_) T(val);
        ++size_;
    }

    void push_back(T&& val) {
        if (size_ == capacity_) grow();
        new(data_ + size_) T(std::move(val));
        ++size_;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == capacity_) grow();
        new(data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back() {
        if (size_ > 0) data_[--size_].~T();
    }

    void clear() {
        for (size_type i = 0; i < size_; ++i) data_[i].~T();
        size_ = 0;
    }

    // 在 pos 前插入（O(n)，触发后移）
    iterator insert(iterator pos, const T& val) {
        size_type idx = pos - data_;
        if (size_ == capacity_) grow();           // grow 可能重新分配
        pos = data_ + idx;
        // 后移腾出位置
        if (size_ > 0) {
            new(data_ + size_) T(std::move(data_[size_ - 1]));
            for (size_type i = size_ - 1; i > idx; --i)
                data_[i] = std::move(data_[i - 1]);
        }
        *pos = val;
        ++size_;
        return pos;
    }

    // 删除 pos 处元素（O(n)，触发前移）
    iterator erase(iterator pos) {
        size_type idx = pos - data_;
        pos->~T();
        for (size_type i = idx; i < size_ - 1; ++i)
            data_[i] = std::move(data_[i + 1]);
        --size_;
        return data_ + idx;
    }

    void swap(Vector& other) noexcept {
        std::swap(data_,     other.data_);
        std::swap(size_,     other.size_);
        std::swap(capacity_, other.capacity_);
    }

private:
    // 扩容：2 倍增长（均摊 O(1) push_back 的关键）
    void grow() {
        reserve(capacity_ == 0 ? 4 : capacity_ * 2);
    }

    T*        data_;
    size_type size_;
    size_type capacity_;
};
