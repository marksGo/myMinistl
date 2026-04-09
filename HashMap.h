#pragma once
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <vector>
#include <optional>
#include <string>

// 开放寻址哈希表（线性探测）
// 核心点：
//   - 负载因子 > 0.75 时触发 rehash（2倍扩容）
//   - 线性探测解决冲突（缓存友好）
//   - 删除用墓碑标记（tombstone），避免破坏探测链
//   - 均摊 O(1) 查找/插入/删除
template <typename K, typename V, typename Hash = std::hash<K>>
class HashMap {
    struct Slot {
        K    key;
        V    value;
        bool occupied  = false;
        bool tombstone = false;  // 已删除的墓碑
    };

public:
    using size_type = std::size_t;

    explicit HashMap(size_type initCap = 16)
        : slots_(initCap), size_(0), tombCount_(0) {}

    // ── 插入 / 更新 ──────────────────────────────────────────────────
    V& operator[](const K& key) {
        maybeRehash();
        size_type idx = findSlot(key);
        if (!slots_[idx].occupied) {
            slots_[idx].key      = key;
            slots_[idx].value    = V{};
            slots_[idx].occupied = true;
            slots_[idx].tombstone = false;
            ++size_;
        }
        return slots_[idx].value;
    }

    void insert(const K& key, const V& val) {
        (*this)[key] = val;
    }

    // ── 查找 ─────────────────────────────────────────────────────────
    V* find(const K& key) {
        size_type idx = probe(key);
        if (idx == NPOS) return nullptr;
        return &slots_[idx].value;
    }

    const V* find(const K& key) const {
        size_type idx = probe(key);
        if (idx == NPOS) return nullptr;
        return &slots_[idx].value;
    }

    bool contains(const K& key) const { return probe(key) != NPOS; }

    V& at(const K& key) {
        V* v = find(key);
        if (!v) throw std::out_of_range("HashMap::at key not found");
        return *v;
    }

    // ── 删除（墓碑标记，不立即整理）────────────────────────────────
    bool erase(const K& key) {
        size_type idx = probe(key);
        if (idx == NPOS) return false;
        slots_[idx].occupied  = false;
        slots_[idx].tombstone = true;
        --size_;
        ++tombCount_;
        // 墓碑过多时 rehash 清理
        if (tombCount_ > slots_.size() / 4) rehash(slots_.size());
        return true;
    }

    // ── 遍历 ─────────────────────────────────────────────────────────
    template <typename Fn>
    void forEach(Fn fn) const {
        for (const auto& s : slots_) {
            if (s.occupied) fn(s.key, s.value);
        }
    }

    size_type size()  const { return size_; }
    bool      empty() const { return size_ == 0; }

    void clear() {
        slots_.assign(slots_.size(), Slot{});
        size_ = tombCount_ = 0;
    }

private:
    static const size_type NPOS = static_cast<size_type>(-1);

    // 线性探测：找 key 所在槽，不存在返回 NPOS
    size_type probe(const K& key) const {
        size_type cap  = slots_.size();
        size_type idx  = Hash{}(key) % cap;
        for (size_type i = 0; i < cap; ++i) {
            size_type cur = (idx + i) % cap;
            const Slot& s = slots_[cur];
            if (!s.occupied && !s.tombstone) return NPOS; // 遇到空槽，后面不可能有
            if (s.occupied && s.key == key)  return cur;
        }
        return NPOS;
    }

    // 找可插入的槽（可以是空槽或墓碑槽）
    size_type findSlot(const K& key) {
        size_type cap      = slots_.size();
        size_type idx      = Hash{}(key) % cap;
        size_type firstTomb = NPOS;
        for (size_type i = 0; i < cap; ++i) {
            size_type cur = (idx + i) % cap;
            Slot& s = slots_[cur];
            if (s.occupied && s.key == key) return cur;  // 已存在
            if (!s.occupied && !s.tombstone) {
                // 空槽：优先用前面遇到的墓碑槽
                return firstTomb != NPOS ? firstTomb : cur;
            }
            if (s.tombstone && firstTomb == NPOS) firstTomb = cur;
        }
        return firstTomb != NPOS ? firstTomb : idx;
    }

    void maybeRehash() {
        // 负载因子 > 0.75 时扩容（size / capacity > 3/4）
        if ((size_ + 1) * 4 > slots_.size() * 3) {
            rehash(slots_.size() * 2);
        }
    }

    void rehash(size_type newCap) {
        std::vector<Slot> old = std::move(slots_);
        slots_.assign(newCap, Slot{});
        size_ = tombCount_ = 0;
        for (auto& s : old) {
            if (s.occupied) insert(s.key, s.value);
        }
    }

    std::vector<Slot> slots_;
    size_type         size_;
    size_type         tombCount_;
};
