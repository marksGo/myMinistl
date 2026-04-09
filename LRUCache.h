#pragma once
#include <unordered_map>
#include <list>
#include <optional>
#include <stdexcept>

// LRU Cache：O(1) get / put
// 实现：双向链表 + 哈希表
//   链表维护访问顺序（头部最近，尾部最久）
//   哈希表存 key → 链表迭代器，实现 O(1) 定位
//
// 面试核心问：
//   为什么要用双向链表？→ 删除节点需要 O(1)，单向链表删除需要找前驱
//   为什么存迭代器而不存指针？→ list 迭代器稳定，不会因插入/删除失效
template <typename K, typename V>
class LRUCache {
    using ListNode = std::pair<K, V>;
    using List     = std::list<ListNode>;
    using Iterator = typename List::iterator;

public:
    explicit LRUCache(int capacity) : cap_(capacity) {
        if (capacity <= 0) throw std::invalid_argument("capacity must > 0");
    }

    // 获取 key 对应的值，不存在返回 nullopt
    // 命中时将节点移到链表头部（最近使用）
    std::optional<V> get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        // 移到头部
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    // 插入或更新
    // 若 key 已存在：更新值，移到头部
    // 若 key 不存在：插入头部，若超容量则淘汰尾部
    void put(const K& key, const V& val) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = val;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        // 超容量先淘汰
        if (static_cast<int>(map_.size()) >= cap_) {
            auto& tail = list_.back();
            map_.erase(tail.first);
            list_.pop_back();
        }
        list_.emplace_front(key, val);
        map_[key] = list_.begin();
    }

    bool     contains(const K& key) const { return map_.count(key) > 0; }
    int      size()     const { return static_cast<int>(map_.size()); }
    int      capacity() const { return cap_; }
    bool     empty()    const { return map_.empty(); }

    // 删除指定 key
    bool erase(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        list_.erase(it->second);
        map_.erase(it);
        return true;
    }

    void clear() { list_.clear(); map_.clear(); }

    // 调试：按访问顺序返回所有 key（最近到最久）
    std::list<K> keys() const {
        std::list<K> result;
        for (auto& [k, v] : list_) result.push_back(k);
        return result;
    }

private:
    int                              cap_;
    List                             list_;   // 双向链表
    std::unordered_map<K, Iterator> map_;    // key → 链表迭代器
};
