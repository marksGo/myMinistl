#pragma once
#include <functional>
#include <optional>
#include <cassert>
#include <vector>

// 红黑树 — std::map 底层数据结构
// 红黑树 5 条性质：
//   1. 每个节点是红色或黑色
//   2. 根节点是黑色
//   3. 叶节点（NIL）是黑色
//   4. 红色节点的两个子节点都是黑色（红节点不相邻）
//   5. 从任意节点到其所有后代叶节点路径上黑色节点数相同（黑高一致）
//
// 插入：新节点染红，通过旋转+染色修复性质 → O(log n)
// 删除：替换节点，通过旋转+染色修复双黑 → O(log n)
template <typename K, typename V, typename Cmp = std::less<K>>
class RBTree {
    enum Color { RED, BLACK };

    struct Node {
        K      key;
        V      value;
        Color  color;
        Node*  left;
        Node*  right;
        Node*  parent;

        Node(const K& k, const V& v, Color c, Node* nil)
            : key(k), value(v), color(c)
            , left(nil), right(nil), parent(nil) {}
    };

public:
    RBTree() : size_(0) {
        nil_       = new Node(K{}, V{}, BLACK, nullptr);
        nil_->left = nil_->right = nil_->parent = nil_;
        root_      = nil_;
    }

    ~RBTree() {
        clear(root_);
        delete nil_;
    }

    // ── 插入 / 更新 ──────────────────────────────────────────────────
    void insert(const K& key, const V& val) {
        Node* z = new Node(key, val, RED, nil_);
        Node* y = nil_;
        Node* x = root_;

        while (x != nil_) {
            y = x;
            if (cmp_(key, x->key))      x = x->left;
            else if (cmp_(x->key, key)) x = x->right;
            else { x->value = val; delete z; return; }  // 更新
        }

        z->parent = y;
        if (y == nil_)            root_   = z;
        else if (cmp_(key, y->key)) y->left  = z;
        else                       y->right = z;

        ++size_;
        insertFixup(z);
    }

    // ── 删除 ─────────────────────────────────────────────────────────
    bool erase(const K& key) {
        Node* z = findNode(key);
        if (z == nil_) return false;
        deleteNode(z);
        --size_;
        return true;
    }

    // ── 查找 ─────────────────────────────────────────────────────────
    std::optional<V> find(const K& key) const {
        Node* n = findNode(key);
        if (n == nil_) return std::nullopt;
        return n->value;
    }

    bool contains(const K& key) const { return findNode(key) != nil_; }

    V& at(const K& key) {
        Node* n = findNode(key);
        if (n == nil_) throw std::out_of_range("RBTree::at");
        return n->value;
    }

    V& operator[](const K& key) {
        Node* n = findNode(key);
        if (n != nil_) return n->value;
        insert(key, V{});
        return findNode(key)->value;
    }

    // ── 有序遍历 ────────────────────────────────────────────────────
    // 中序遍历，回调按 key 升序触发
    template <typename Fn>
    void inorder(Fn fn) const { inorderImpl(root_, fn); }

    // 返回所有 kv 对（有序）
    std::vector<std::pair<K, V>> toVector() const {
        std::vector<std::pair<K, V>> result;
        inorder([&](const K& k, const V& v) {
            result.emplace_back(k, v);
        });
        return result;
    }

    // 最小/最大 key
    std::optional<K> minKey() const {
        if (root_ == nil_) return std::nullopt;
        return minimum(root_)->key;
    }
    std::optional<K> maxKey() const {
        if (root_ == nil_) return std::nullopt;
        return maximum(root_)->key;
    }

    size_t size()  const { return size_; }
    bool   empty() const { return size_ == 0; }

private:
    // ── 旋转 ─────────────────────────────────────────────────────────
    //       y                x
    //      / \    右旋      / \
    //     x   c  ——————>  a   y
    //    / \              / \
    //   a   b            b   c
    void rotateLeft(Node* x) {
        Node* y  = x->right;
        x->right = y->left;
        if (y->left != nil_) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_)            root_          = y;
        else if (x == x->parent->left)    x->parent->left  = y;
        else                              x->parent->right = y;
        y->left   = x;
        x->parent = y;
    }

    void rotateRight(Node* y) {
        Node* x  = y->left;
        y->left  = x->right;
        if (x->right != nil_) x->right->parent = y;
        x->parent = y->parent;
        if (y->parent == nil_)            root_          = x;
        else if (y == y->parent->left)    y->parent->left  = x;
        else                              y->parent->right = x;
        x->right  = y;
        y->parent = x;
    }

    // ── 插入修复（维护红黑性质）──────────────────────────────────────
    // 情况 1：叔父是红色 → 父+叔染黑，祖父染红，祖父作为新 z 继续修复
    // 情况 2：叔父是黑色，z 是内侧子节点 → 旋转转化为情况 3
    // 情况 3：叔父是黑色，z 是外侧子节点 → 旋转+染色
    void insertFixup(Node* z) {
        while (z->parent->color == RED) {
            if (z->parent == z->parent->parent->left) {
                Node* y = z->parent->parent->right;   // 叔父
                if (y->color == RED) {                  // case 1
                    z->parent->color          = BLACK;
                    y->color                  = BLACK;
                    z->parent->parent->color  = RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {        // case 2 → case 3
                        z = z->parent;
                        rotateLeft(z);
                    }
                    z->parent->color         = BLACK;   // case 3
                    z->parent->parent->color = RED;
                    rotateRight(z->parent->parent);
                }
            } else {   // 镜像
                Node* y = z->parent->parent->left;
                if (y->color == RED) {
                    z->parent->color          = BLACK;
                    y->color                  = BLACK;
                    z->parent->parent->color  = RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rotateRight(z);
                    }
                    z->parent->color         = BLACK;
                    z->parent->parent->color = RED;
                    rotateLeft(z->parent->parent);
                }
            }
        }
        root_->color = BLACK;   // 性质 2
    }

    // ── 删除 ─────────────────────────────────────────────────────────
    void transplant(Node* u, Node* v) {
        if (u->parent == nil_)         root_          = v;
        else if (u == u->parent->left) u->parent->left  = v;
        else                           u->parent->right = v;
        v->parent = u->parent;
    }

    void deleteNode(Node* z) {
        Node* y = z;
        Node* x;
        Color yOrigColor = y->color;

        if (z->left == nil_) {
            x = z->right;
            transplant(z, z->right);
        } else if (z->right == nil_) {
            x = z->left;
            transplant(z, z->left);
        } else {
            y = minimum(z->right);   // 后继节点
            yOrigColor = y->color;
            x = y->right;
            if (y->parent == z) {
                x->parent = y;
            } else {
                transplant(y, y->right);
                y->right         = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left         = z->left;
            y->left->parent = y;
            y->color        = z->color;
        }
        delete z;
        if (yOrigColor == BLACK) deleteFixup(x);
    }

    // ── 删除修复（消除双黑）─────────────────────────────────────────
    void deleteFixup(Node* x) {
        while (x != root_ && x->color == BLACK) {
            if (x == x->parent->left) {
                Node* w = x->parent->right;
                if (w->color == RED) {                       // case 1
                    w->color         = BLACK;
                    x->parent->color = RED;
                    rotateLeft(x->parent);
                    w = x->parent->right;
                }
                if (w->left->color == BLACK && w->right->color == BLACK) {  // case 2
                    w->color = RED;
                    x = x->parent;
                } else {
                    if (w->right->color == BLACK) {          // case 3
                        w->left->color = BLACK;
                        w->color       = RED;
                        rotateRight(w);
                        w = x->parent->right;
                    }
                    w->color           = x->parent->color;   // case 4
                    x->parent->color   = BLACK;
                    w->right->color    = BLACK;
                    rotateLeft(x->parent);
                    x = root_;
                }
            } else {   // 镜像
                Node* w = x->parent->left;
                if (w->color == RED) {
                    w->color         = BLACK;
                    x->parent->color = RED;
                    rotateRight(x->parent);
                    w = x->parent->left;
                }
                if (w->right->color == BLACK && w->left->color == BLACK) {
                    w->color = RED;
                    x = x->parent;
                } else {
                    if (w->left->color == BLACK) {
                        w->right->color = BLACK;
                        w->color        = RED;
                        rotateLeft(w);
                        w = x->parent->left;
                    }
                    w->color           = x->parent->color;
                    x->parent->color   = BLACK;
                    w->left->color     = BLACK;
                    rotateRight(x->parent);
                    x = root_;
                }
            }
        }
        x->color = BLACK;
    }

    // ── 辅助 ────────────────────────────────────────────────────────
    Node* findNode(const K& key) const {
        Node* x = root_;
        while (x != nil_) {
            if      (cmp_(key, x->key)) x = x->left;
            else if (cmp_(x->key, key)) x = x->right;
            else return x;
        }
        return nil_;
    }

    Node* minimum(Node* x) const {
        while (x->left != nil_) x = x->left;
        return x;
    }

    Node* maximum(Node* x) const {
        while (x->right != nil_) x = x->right;
        return x;
    }

    template <typename Fn>
    void inorderImpl(Node* x, Fn& fn) const {
        if (x == nil_) return;
        inorderImpl(x->left, fn);
        fn(x->key, x->value);
        inorderImpl(x->right, fn);
    }

    void clear(Node* x) {
        if (x == nil_) return;
        clear(x->left);
        clear(x->right);
        delete x;
    }

    Node*  nil_;    // 哨兵叶节点（黑色），避免大量 nullptr 判断
    Node*  root_;
    size_t size_;
    Cmp    cmp_;
};
