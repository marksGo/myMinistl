#pragma once
#include <vector>
#include <functional>
#include <stdexcept>

// 二叉堆（默认最小堆）
// 底层用数组存储完全二叉树：
//   parent(i) = (i-1)/2
//   left(i)   = 2*i+1
//   right(i)  = 2*i+2
//
// push O(log n)  — 上浮 siftUp
// pop  O(log n)  — 末尾替换堆顶，下沉 siftDown
// top  O(1)
// 传 std::greater<T> 即为最大堆
template <typename T, typename Cmp = std::less<T>>
class Heap {
public:
    explicit Heap(Cmp cmp = Cmp()) : cmp_(cmp) {}

    // 从已有数组建堆，O(n) Floyd 算法
    Heap(std::vector<T> data, Cmp cmp = Cmp())
        : data_(std::move(data)), cmp_(cmp) {
        buildHeap();
    }

    void push(const T& val) {
        data_.push_back(val);
        siftUp(data_.size() - 1);
    }

    void push(T&& val) {
        data_.push_back(std::move(val));
        siftUp(data_.size() - 1);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
        siftUp(data_.size() - 1);
    }

    // 弹出堆顶
    void pop() {
        if (empty()) throw std::runtime_error("Heap::pop on empty heap");
        data_[0] = std::move(data_.back());
        data_.pop_back();
        if (!empty()) siftDown(0);
    }

    const T& top() const {
        if (empty()) throw std::runtime_error("Heap::top on empty heap");
        return data_[0];
    }

    bool   empty() const { return data_.empty(); }
    size_t size()  const { return data_.size(); }

    // 将堆中等于 val 的第一个元素修改为 newVal（O(n) 查找 + O(log n) 调整）
    bool update(const T& val, const T& newVal) {
        for (size_t i = 0; i < data_.size(); ++i) {
            if (data_[i] == val) {
                data_[i] = newVal;
                siftUp(i);
                siftDown(i);
                return true;
            }
        }
        return false;
    }

private:
    // Floyd 建堆：从最后一个非叶节点开始向上 siftDown，O(n)
    void buildHeap() {
        if (data_.size() <= 1) return;
        for (int i = static_cast<int>(data_.size() / 2) - 1; i >= 0; --i)
            siftDown(static_cast<size_t>(i));
    }

    // 上浮：新插入元素与父节点比较，违反堆性质则交换
    void siftUp(size_t i) {
        while (i > 0) {
            size_t p = (i - 1) / 2;
            if (cmp_(data_[i], data_[p])) {   // 子 < 父（最小堆）→ 上浮
                std::swap(data_[i], data_[p]);
                i = p;
            } else break;
        }
    }

    // 下沉：与较小的子节点交换，直到满足堆性质
    void siftDown(size_t i) {
        size_t n = data_.size();
        while (true) {
            size_t smallest = i;
            size_t l = 2 * i + 1, r = 2 * i + 2;
            if (l < n && cmp_(data_[l], data_[smallest])) smallest = l;
            if (r < n && cmp_(data_[r], data_[smallest])) smallest = r;
            if (smallest == i) break;
            std::swap(data_[i], data_[smallest]);
            i = smallest;
        }
    }

    std::vector<T> data_;
    Cmp            cmp_;
};

// 别名
template <typename T> using MinHeap = Heap<T, std::less<T>>;
template <typename T> using MaxHeap = Heap<T, std::greater<T>>;
