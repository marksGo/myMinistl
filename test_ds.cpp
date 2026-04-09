#include "LRUCache.h"
#include "MinHeap.h"
#include "SkipList.h"
#include "RBTree.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <map>
#include <queue>

using Clock = std::chrono::high_resolution_clock;
using Us    = std::chrono::microseconds;
static long us(Clock::time_point t) {
    return std::chrono::duration_cast<Us>(Clock::now() - t).count();
}

// ── LRU Cache ────────────────────────────────────────────────────────
void testLRU() {
    printf("\n=== LRU Cache ===\n");

    LRUCache<int, std::string> cache(3);
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    assert(cache.get(1).value() == "one");   // 访问 1，顺序: 1,3,2
    cache.put(4, "four");                    // 淘汰最久未用的 2
    assert(!cache.get(2).has_value());       // 2 已被淘汰
    assert(cache.get(3).value() == "three");
    assert(cache.get(4).value() == "four");
    assert(cache.size() == 3);

    // 更新已有 key
    cache.put(3, "THREE");
    assert(cache.get(3).value() == "THREE");

    // erase
    assert(cache.erase(4));
    assert(!cache.contains(4));
    assert(cache.size() == 2);

    printf("  正确性: PASS\n");

    // 性能
    LRUCache<int, int> pc(10000);
    const int N = 1'000'000;
    auto t = Clock::now();
    for (int i = 0; i < N; ++i) {
        pc.put(i % 10000, i);
        pc.get(i % 9999);
    }
    printf("  %dw次 put+get: %ldus\n", N/10000, us(t));
}

// ── MinHeap / MaxHeap ────────────────────────────────────────────────
void testHeap() {
    printf("\n=== MinHeap / MaxHeap ===\n");

    // 最小堆
    MinHeap<int> h;
    for (int x : {5, 3, 8, 1, 9, 2, 7}) h.push(x);
    std::vector<int> sorted;
    while (!h.empty()) { sorted.push_back(h.top()); h.pop(); }
    assert(std::is_sorted(sorted.begin(), sorted.end()));
    printf("  MinHeap 排序: PASS  %s\n", [&]{
        std::string s;
        for (int x : sorted) s += std::to_string(x) + " ";
        return s;
    }().c_str());

    // 最大堆
    MaxHeap<int> mh;
    for (int x : {5, 3, 8, 1, 9, 2, 7}) mh.push(x);
    assert(mh.top() == 9);
    printf("  MaxHeap top=9: PASS\n");

    // Floyd O(n) 建堆
    MinHeap<int> bh({9, 7, 5, 3, 1, 2, 4, 6, 8});
    std::vector<int> sorted2;
    while (!bh.empty()) { sorted2.push_back(bh.top()); bh.pop(); }
    assert(std::is_sorted(sorted2.begin(), sorted2.end()));
    printf("  Floyd buildHeap: PASS\n");

    // 性能对比
    const int N = 1'000'000;
    auto t1 = Clock::now();
    MinHeap<int> mheap;
    for (int i = N; i >= 0; --i) mheap.push(i);
    while (!mheap.empty()) mheap.pop();
    long myTime = us(t1);

    auto t2 = Clock::now();
    std::priority_queue<int, std::vector<int>, std::greater<int>> pq;
    for (int i = N; i >= 0; --i) pq.push(i);
    while (!pq.empty()) pq.pop();
    long stdTime = us(t2);

    printf("  push+pop %dw次: mini=%ldus  std=%ldus\n", N/10000, myTime, stdTime);
}

// ── SkipList ─────────────────────────────────────────────────────────
void testSkipList() {
    printf("\n=== SkipList ===\n");

    SkipList<int, std::string> sl;
    for (auto& [k, v] : std::vector<std::pair<int,std::string>>{
            {10,"ten"},{3,"three"},{7,"seven"},{1,"one"},{5,"five"}})
        sl.insert(k, v);

    assert(sl.size() == 5);
    assert(sl.find(7).value() == "seven");
    assert(!sl.find(99).has_value());

    // rank 查询
    assert(sl.rank(1) == 1);
    assert(sl.rank(10) == 5);
    auto [rk, rv] = sl.getByRank(3).value();
    assert(rk == 5 && rv == "five");
    printf("  rank 查询: PASS\n");

    // 范围查询
    auto range = sl.range(3, 7);
    assert(range.size() == 3);
    assert(range[0].first == 3 && range[2].first == 7);
    printf("  range [3,7]: PASS  ");
    for (auto& [k, v] : range) printf("%d ", k);
    printf("\n");

    // 更新
    sl.insert(7, "SEVEN");
    assert(sl.find(7).value() == "SEVEN");

    // 删除
    assert(sl.erase(3));
    assert(!sl.contains(3));
    assert(sl.size() == 4);
    printf("  删除+更新: PASS\n");

    // 性能对比
    const int N = 500'000;
    SkipList<int, int> ssl;
    auto t1 = Clock::now();
    for (int i = 0; i < N; ++i) ssl.insert(i, i);
    for (int i = 0; i < N; ++i) assert(ssl.find(i).value() == i);
    long myTime = us(t1);

    std::map<int, int> sm;
    auto t2 = Clock::now();
    for (int i = 0; i < N; ++i) sm[i] = i;
    for (int i = 0; i < N; ++i) assert(sm[i] == i);
    long stdTime = us(t2);

    printf("  insert+find %dw次: SkipList=%ldus  std::map=%ldus\n",
           N/10000, myTime, stdTime);
}

// ── RBTree ───────────────────────────────────────────────────────────
void testRBTree() {
    printf("\n=== RBTree ===\n");

    RBTree<int, std::string> t;
    for (auto& [k, v] : std::vector<std::pair<int,std::string>>{
            {5,"e"},{3,"c"},{7,"g"},{1,"a"},{4,"d"},{6,"f"},{8,"h"},{2,"b"}})
        t.insert(k, v);

    assert(t.size() == 8);
    assert(t.find(4).value() == "d");
    assert(!t.find(99).has_value());

    // 中序遍历验证有序
    auto vec = t.toVector();
    assert(std::is_sorted(vec.begin(), vec.end(),
           [](auto& a, auto& b){ return a.first < b.first; }));
    printf("  中序遍历有序: PASS  ");
    for (auto& [k, v] : vec) printf("%d ", k);
    printf("\n");

    assert(t.minKey().value() == 1);
    assert(t.maxKey().value() == 8);

    // operator[]
    t[9] = "i";
    assert(t.find(9).value() == "i");
    assert(t.size() == 9);

    // 删除（覆盖三种情况：叶节点、单子节点、双子节点）
    assert(t.erase(1));   // 叶节点
    assert(t.erase(7));   // 双子节点
    assert(t.erase(3));   // 单子节点
    assert(t.size() == 6);
    assert(!t.contains(1) && !t.contains(7) && !t.contains(3));

    auto vec2 = t.toVector();
    assert(std::is_sorted(vec2.begin(), vec2.end(),
           [](auto& a, auto& b){ return a.first < b.first; }));
    printf("  删除后仍有序: PASS  ");
    for (auto& [k, v] : vec2) printf("%d ", k);
    printf("\n");

    // 性能对比
    const int N = 500'000;
    RBTree<int, int> rt;
    auto t1 = Clock::now();
    for (int i = 0; i < N; ++i) rt.insert(i, i);
    for (int i = 0; i < N; ++i) assert(rt.find(i).value() == i);
    long myTime = us(t1);

    std::map<int, int> sm;
    auto t2 = Clock::now();
    for (int i = 0; i < N; ++i) sm[i] = i;
    for (int i = 0; i < N; ++i) assert(sm[i] == i);
    long stdTime = us(t2);

    printf("  insert+find %dw次: RBTree=%ldus  std::map=%ldus\n",
           N/10000, myTime, stdTime);
}

int main() {
    printf("========== 数据结构测试 ==========\n");
    testLRU();
    testHeap();
    testSkipList();
    testRBTree();
    printf("\n========== All tests passed ==========\n");
    return 0;
}
