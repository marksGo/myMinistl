# mini-STL — C++ 手写基础组件与数据结构

从零实现内存池、STL 容器、LRU 缓存、堆、跳表、红黑树，用于理解底层原理。

## 文件列表

基础组件
  MemoryPool.h   Slab 风格内存池，按固定块大小预分配，空闲链表 O(1) 分配/释放
  Allocator.h    符合 STL allocator 接口，可接入 std::vector / std::list 等容器
  Vector.h       动态数组，2倍扩容，支持迭代器 insert erase emplace_back
  HashMap.h      开放寻址哈希表，线性探测，负载因子 0.75 触发 rehash，墓碑标记删除
  SharedPtr.h    引用计数智能指针，atomic 线程安全，WeakPtr 解决循环引用

数据结构
  LRUCache.h     LRU 缓存，双向链表 + 哈希表，O(1) get / put
  MinHeap.h      二叉堆，支持最小堆/最大堆，O(n) Floyd 建堆，O(log n) push/pop
  SkipList.h     跳表，Redis ZSet 底层结构，支持 rank 查询和范围查询，O(log n)
  RBTree.h       红黑树，std::map 底层结构，中序遍历有序，O(log n) 插入/删除/查找

## 编译运行

基础组件性能测试
  g++ -std=c++17 -O2 -pthread benchmark.cpp -o benchmark && ./benchmark

数据结构正确性 + 性能测试
  g++ -std=c++17 -O2 -pthread test_ds.cpp -o test_ds && ./test_ds

## 性能数据（实测）

MemoryPool   100w次 construct/destroy   较 new/delete 提速 2.7x
Vector       100w次 push_back           749us vs std 2567us
MinHeap      100w次 push+pop            较 std::priority_queue 快 1.5x
RBTree       50w次  insert+find         较 std::map 快 1.5x
LRU Cache    100w次 put+get             10873us
SkipList     50w次  insert+find         接近 std::map（概率结构有波动）

## 设计要点

MemoryPool
  预申请大块内存切成等大 chunk，空闲块头部存下一个空闲块地址形成链表
  分配取链表头，释放插回链表头，均 O(1)，避免频繁系统调用

Vector
  capacity 不足时申请 2 倍新内存并 move 原元素，扩容后所有迭代器失效

HashMap
  哈希取模定位槽，冲突时线性向后探测（缓存友好）
  删除用墓碑标记而非直接置空，否则截断探测链导致查找失败
  墓碑超过 capacity/4 时触发 rehash 清理

SharedPtr / WeakPtr
  强引用归零析构对象，强+弱均归零释放控制块
  WeakPtr::lock() 用 CAS 原子提升 strongCount，防止并发析构
  循环引用场景用 WeakPtr 替换其中一方避免泄漏

LRU Cache
  双向链表维护访问顺序（头部最近，尾部最久），哈希表存 key 到迭代器的映射
  用双向链表而非单向链表：删除节点需要 O(1) 找前驱

MinHeap
  Floyd 建堆 O(n)：从最后一个非叶节点向上 siftDown，底层节点下沉距离短
  插入最多旋转 2 次（与红黑树类似）

SkipList
  多层有序链表，每个节点随机决定层数（几何分布 p=0.5）
  每层维护 span 字段，支持 O(log n) 按排名查找
  Redis 选跳表而非红黑树：实现简单、范围查询直观、并发改造容易

RBTree
  5 条性质保证黑高一致，树高 O(log n)
  哨兵 nil 节点（黑色）简化边界判断，避免大量 nullptr 检查
  插入修复最多 2 次旋转，删除修复最多 3 次旋转
