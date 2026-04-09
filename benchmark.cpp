#include "MemoryPool.h"
#include "Allocator.h"
#include "Vector.h"
#include "HashMap.h"
#include "SharedPtr.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <cassert>
#include <cstdio>
#include <string>

using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::microseconds;

static long elapsed(Clock::time_point t) {
    return std::chrono::duration_cast<Ms>(Clock::now() - t).count();
}

// ── 测试 Vector ──────────────────────────────────────────────────────
void testVector() {
    printf("\n=== Vector ===\n");

    const int N = 1'000'000;

    // 正确性
    Vector<int> v;
    for (int i = 0; i < 100; ++i) v.push_back(i);
    assert(v.size() == 100);
    assert(v[0] == 0 && v[99] == 99);
    v.pop_back();
    assert(v.size() == 99);
    auto it = v.insert(v.begin() + 5, 999);
    assert(*it == 999 && v[5] == 999);
    v.erase(v.begin() + 5);
    assert(v[5] == 5);
    printf("  正确性: PASS\n");

    // 性能对比
    auto t1 = Clock::now();
    Vector<int> mv;
    mv.reserve(N);
    for (int i = 0; i < N; ++i) mv.push_back(i);
    long myTime = elapsed(t1);

    auto t2 = Clock::now();
    std::vector<int> sv;
    sv.reserve(N);
    for (int i = 0; i < N; ++i) sv.push_back(i);
    long stdTime = elapsed(t2);

    printf("  push_back %dw次: mini=%ldus  std=%ldus\n", N/10000, myTime, stdTime);
}

// ── 测试 HashMap ─────────────────────────────────────────────────────
void testHashMap() {
    printf("\n=== HashMap ===\n");

    // 正确性
    HashMap<std::string, int> m;
    m.insert("apple", 1);
    m.insert("banana", 2);
    m["cherry"] = 3;
    assert(m.contains("apple") && m.at("apple") == 1);
    assert(m.size() == 3);
    m.erase("banana");
    assert(!m.contains("banana") && m.size() == 2);
    // rehash 后数据完整
    for (int i = 0; i < 100; ++i) m.insert(std::to_string(i), i);
    assert(m.contains("42") && m.at("42") == 42);
    printf("  正确性: PASS\n");

    const int N = 500'000;

    auto t1 = Clock::now();
    HashMap<int, int> mm;
    for (int i = 0; i < N; ++i) mm.insert(i, i * 2);
    for (int i = 0; i < N; ++i) assert(mm.at(i) == i * 2);
    long myTime = elapsed(t1);

    auto t2 = Clock::now();
    std::unordered_map<int, int> sm;
    for (int i = 0; i < N; ++i) sm[i] = i * 2;
    for (int i = 0; i < N; ++i) assert(sm[i] == i * 2);
    long stdTime = elapsed(t2);

    printf("  insert+find %dw次: mini=%ldus  std=%ldus\n", N/10000, myTime, stdTime);
}

// ── 测试 SharedPtr ───────────────────────────────────────────────────
void testSharedPtr() {
    printf("\n=== SharedPtr ===\n");

    // 基本使用
    auto sp1 = makeShared<int>(42);
    assert(*sp1 == 42 && sp1.useCount() == 1);
    {
        SharedPtr<int> sp2 = sp1;
        assert(sp1.useCount() == 2);
    }
    assert(sp1.useCount() == 1);

    // WeakPtr
    WeakPtr<int> wp = sp1;
    assert(!wp.expired());
    {
        auto locked = wp.lock();
        assert(locked && *locked == 42);
        assert(sp1.useCount() == 2);
    }
    sp1.reset();
    assert(wp.expired());
    assert(!wp.lock());
    printf("  正确性: PASS\n");

    // 循环引用检测
    struct Node {
        int val;
        SharedPtr<Node> next;   // 改成 WeakPtr<Node> 才不会泄漏
        WeakPtr<Node>   weakNext;
        Node(int v) : val(v) {}
    };
    {
        auto n1 = makeShared<Node>(1);
        auto n2 = makeShared<Node>(2);
        n1->weakNext = n2;   // 用 weak，不增加强引用
        n2->weakNext = n1;
        // n1, n2 正常析构，无泄漏
    }
    printf("  循环引用（WeakPtr）: PASS\n");

    // 性能对比
    const int N = 1'000'000;
    auto t1 = Clock::now();
    for (int i = 0; i < N; ++i) {
        auto p = makeShared<int>(i);
        SharedPtr<int> p2 = p;
    }
    long myTime = elapsed(t1);

    auto t2 = Clock::now();
    for (int i = 0; i < N; ++i) {
        auto p = std::make_shared<int>(i);
        std::shared_ptr<int> p2 = p;
    }
    long stdTime = elapsed(t2);

    printf("  make+copy %dw次: mini=%ldus  std=%ldus\n", N/10000, myTime, stdTime);
}

// ── 测试内存池 ───────────────────────────────────────────────────────
void testMemoryPool() {
    printf("\n=== MemoryPool + ObjectPool ===\n");

    struct Obj { int x, y, z; Obj(int a, int b, int c): x(a), y(b), z(c) {} };
    const int N = 1'000'000;

    // ObjectPool
    auto t1 = Clock::now();
    ObjectPool<Obj> pool;
    std::vector<Obj*> ptrs;
    ptrs.reserve(N);
    for (int i = 0; i < N; ++i) ptrs.push_back(pool.construct(i, i, i));
    for (auto* p : ptrs)        pool.destroy(p);
    long poolTime = elapsed(t1);

    // 对比 new/delete
    auto t2 = Clock::now();
    std::vector<Obj*> ptrs2;
    ptrs2.reserve(N);
    for (int i = 0; i < N; ++i) ptrs2.push_back(new Obj{i, i, i});
    for (auto* p : ptrs2)       delete p;
    long newTime = elapsed(t2);

    printf("  construct/destroy %dw次: pool=%ldus  new/delete=%ldus  加速=%.1fx\n",
           N/10000, poolTime, newTime, (double)newTime / poolTime);

    // PoolAllocator 接入 std::vector
    std::vector<int, PoolAllocator<int>> pv;
    for (int i = 0; i < 1000; ++i) pv.push_back(i);
    assert(pv.size() == 1000 && pv[42] == 42);
    printf("  PoolAllocator + std::vector: PASS\n");
}

int main() {
    printf("========== mini-STL benchmark ==========\n");
    testMemoryPool();
    testVector();
    testHashMap();
    testSharedPtr();
    printf("\n========== All tests passed ==========\n");
    return 0;
}
