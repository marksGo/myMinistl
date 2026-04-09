# mini-STL — C++ 手写基础组件

从零实现内存池、动态数组、哈希表、智能指针，用于理解 STL 底层原理。

## 模块

MemoryPool   Slab 风格内存池，按固定块大小预分配，空闲链表管理，O(1) 分配/释放
ObjectPool   在 MemoryPool 基础上封装构造/析构，支持任意对象类型
Allocator    符合 STL allocator 接口，可直接接入 std::vector / std::list 等容器
Vector       动态数组，2倍扩容策略，支持迭代器、insert、erase、emplace_back
HashMap      开放寻址哈希表，线性探测解决冲突，负载因子 0.75 触发 rehash，墓碑标记删除
SharedPtr    引用计数智能指针，atomic 保证线程安全
WeakPtr      弱引用，不增加强引用计数，lock() 安全提升为 SharedPtr，解决循环引用

## 编译运行

g++ -std=c++17 -O2 -pthread benchmark.cpp -o benchmark
./benchmark

## 性能数据（本机实测）

MemoryPool  100w次 construct/destroy  较 new/delete 提速 2.7x
Vector      100w次 push_back          749us vs std 2567us
HashMap     50w次  insert+find        接近 std（开放寻址缓存友好）
SharedPtr   全部正确性测试            PASS

## 设计要点

内存池
  预申请大块内存切成等大 chunk，空闲块头部存下一个空闲块地址形成链表
  分配时取链表头，释放时插回链表头，均为 O(1)，避免频繁系统调用

Vector
  capacity 不足时申请 2 倍新内存，move 原有元素，释放旧内存
  扩容后所有迭代器失效

HashMap
  哈希值取模定位槽，冲突时线性向后探测
  删除用墓碑标记而非直接置空，否则会截断探测链导致查找失败
  墓碑数量超过 capacity/4 时触发 rehash 清理

SharedPtr / WeakPtr
  强引用归零时析构对象，强+弱引用均归零时释放控制块
  WeakPtr::lock() 用 CAS 原子地将 strongCount 从 N 加到 N+1，防止并发析构
  循环引用场景用 WeakPtr 替换其中一方即可避免泄漏# myMinistl
