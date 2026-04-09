#pragma once
#include <cstddef>
#include <random>
#include <optional>
#include <vector>
#include <cassert>

// 跳表 SkipList
// Redis ZSet 底层数据结构，支持 O(log n) 插入/删除/查找，以及 rank 查询
//
// 结构：多层有序链表
//   Level 3: head -----------------> [30] -------> tail
//   Level 2: head -------> [15] ---> [30] -------> tail
//   Level 1: head -> [5] -> [15] --> [30] -> [40] -> tail
//
// 每个节点随机决定层数（几何分布，p=0.5），期望层数 O(log n)
// 每层维护 span（跨越的节点数），支持 O(log n) 按排名查找
template <typename K, typename V>
class SkipList {
    static const int MAX_LEVEL = 16;
    static constexpr double P  = 0.5;

    struct Node {
        K key;
        V value;
        struct Level {
            Node*  forward = nullptr; // 同层下一个节点
            size_t span    = 0;       // 到 forward 跨越的节点数（用于 rank）
        };
        std::vector<Level> levels;

        Node(const K& k, const V& v, int h)
            : key(k), value(v), levels(h) {}
    };

public:
    SkipList() : size_(0), level_(1) {
        // head 是哨兵节点，不存储实际数据
        head_ = new Node(K{}, V{}, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; ++i)
            head_->levels[i].span = 0;
    }

    ~SkipList() {
        Node* cur = head_;
        while (cur) {
            Node* next = cur->levels[0].forward;
            delete cur;
            cur = next;
        }
    }

    // ── 插入 / 更新 O(log n) ──────────────────────────────────────────
    void insert(const K& key, const V& val) {
        // update[i]：第 i 层中，key 前面的那个节点
        // rank[i]：到 update[i] 为止经过的节点数
        Node* update[MAX_LEVEL];
        size_t rank[MAX_LEVEL] = {};

        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            rank[i] = (i == level_ - 1) ? 0 : rank[i + 1];
            while (cur->levels[i].forward &&
                   cur->levels[i].forward->key < key) {
                rank[i] += cur->levels[i].span;
                cur = cur->levels[i].forward;
            }
            // 如果 key 已存在，直接更新 value
            if (cur->levels[i].forward &&
                cur->levels[i].forward->key == key) {
                cur->levels[i].forward->value = val;
                return;
            }
            update[i] = cur;
        }

        int newLevel = randomLevel();
        if (newLevel > level_) {
            for (int i = level_; i < newLevel; ++i) {
                rank[i]          = 0;
                update[i]        = head_;
                update[i]->levels[i].span = size_;
            }
            level_ = newLevel;
        }

        Node* node = new Node(key, val, newLevel);
        for (int i = 0; i < newLevel; ++i) {
            node->levels[i].forward        = update[i]->levels[i].forward;
            update[i]->levels[i].forward   = node;
            // 更新 span
            node->levels[i].span           = update[i]->levels[i].span - (rank[0] - rank[i]);
            update[i]->levels[i].span      = rank[0] - rank[i] + 1;
        }
        // 高层 update 节点的 span +1（新节点插在它们覆盖范围内）
        for (int i = newLevel; i < level_; ++i)
            update[i]->levels[i].span++;

        ++size_;
    }

    // ── 删除 O(log n) ────────────────────────────────────────────────
    bool erase(const K& key) {
        Node* update[MAX_LEVEL];
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->levels[i].forward &&
                   cur->levels[i].forward->key < key)
                cur = cur->levels[i].forward;
            update[i] = cur;
        }

        Node* target = cur->levels[0].forward;
        if (!target || target->key != key) return false;

        for (int i = 0; i < level_; ++i) {
            if (update[i]->levels[i].forward != target) {
                update[i]->levels[i].span--;
                continue;
            }
            update[i]->levels[i].span    += target->levels[i].span - 1;
            update[i]->levels[i].forward  = target->levels[i].forward;
        }
        delete target;
        --size_;

        // 缩减 level_
        while (level_ > 1 && !head_->levels[level_ - 1].forward) --level_;
        return true;
    }

    // ── 查找 O(log n) ────────────────────────────────────────────────
    std::optional<V> find(const K& key) const {
        const Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i)
            while (cur->levels[i].forward &&
                   cur->levels[i].forward->key < key)
                cur = cur->levels[i].forward;
        const Node* node = cur->levels[0].forward;
        if (node && node->key == key) return node->value;
        return std::nullopt;
    }

    bool contains(const K& key) const { return find(key).has_value(); }

    // ── 按排名查找 O(log n)（1-based）────────────────────────────────
    std::optional<std::pair<K, V>> getByRank(size_t rank) const {
        if (rank == 0 || rank > size_) return std::nullopt;
        size_t      traversed = 0;
        const Node* cur       = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->levels[i].forward &&
                   traversed + cur->levels[i].span <= rank) {
                traversed += cur->levels[i].span;
                cur = cur->levels[i].forward;
            }
            if (traversed == rank)
                return std::make_pair(cur->key, cur->value);
        }
        return std::nullopt;
    }

    // key 的排名（1-based），不存在返回 0
    size_t rank(const K& key) const {
        size_t      r   = 0;
        const Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->levels[i].forward &&
                   cur->levels[i].forward->key <= key) {
                r  += cur->levels[i].span;
                cur = cur->levels[i].forward;
                if (cur->key == key) return r;
            }
        }
        return 0;
    }

    // 范围查询 [minKey, maxKey]，O(log n + k)
    std::vector<std::pair<K, V>> range(const K& minKey, const K& maxKey) const {
        std::vector<std::pair<K, V>> result;
        const Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i)
            while (cur->levels[i].forward &&
                   cur->levels[i].forward->key < minKey)
                cur = cur->levels[i].forward;
        cur = cur->levels[0].forward;
        while (cur && cur->key <= maxKey) {
            result.emplace_back(cur->key, cur->value);
            cur = cur->levels[0].forward;
        }
        return result;
    }

    size_t size()  const { return size_; }
    bool   empty() const { return size_ == 0; }
    int    level() const { return level_; }

private:
    int randomLevel() {
        static std::mt19937 rng{std::random_device{}()};
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        int lvl = 1;
        while (dist(rng) < P && lvl < MAX_LEVEL) ++lvl;
        return lvl;
    }

    Node*  head_;
    size_t size_;
    int    level_;  // 当前最高层数
};
