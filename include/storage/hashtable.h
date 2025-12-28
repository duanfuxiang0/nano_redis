#pragma once

#include <vector>
#include <functional>
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdint>

template <typename K, typename V>
class HashTable {
private:
    struct Node {
        K key;
        V value;
        uint64_t hash;
        Node *next;

        template <typename KeyArg, typename ValArg>
        Node(KeyArg &&k, ValArg &&v, uint64_t h, Node *n)
            : key(std::forward<KeyArg>(k)),
              value(std::forward<ValArg>(v)),
              hash(h),
              next(n) {}
    };

public:
    explicit HashTable(uint64_t initial_cap = 4)
        : list_(nullptr), length_(0), size_(0)
    {
        uint64_t min_cap = 4;
        while (min_cap < initial_cap) {
            min_cap *= 2;
        }
        Resize(min_cap);
    }

    ~HashTable() {
        Clear();
        delete[] list_;
    }

    HashTable(const HashTable &) = delete;
    HashTable &operator=(const HashTable &) = delete;

    HashTable(HashTable &&other) noexcept
        : list_(other.list_), length_(other.length_), size_(other.size_) {
        other.list_ = nullptr;
        other.length_ = 0;
        other.size_ = 0;
    }

    HashTable& operator=(HashTable&& other) noexcept {
        if (this != &other) {
            Clear();
            delete[] list_;

            list_ = other.list_;
            length_ = other.length_;
            size_ = other.size_;

            other.list_ = nullptr;
            other.length_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    template <typename KeyArg, typename ValArg>
    void Insert(KeyArg &&key, ValArg &&value) {
        if (list_ == nullptr || length_ == 0) {
            uint64_t min_cap = 4;
            Resize(min_cap);
        }
        
        uint64_t hash = Hash(key);
        Node **ptr = FindPointer(key, hash);

        if (*ptr != nullptr) {
            (*ptr)->value = std::forward<ValArg>(value);
        } else {
            Node *old_next = *ptr;
            *ptr = new Node(std::forward<KeyArg>(key), std::forward<ValArg>(value), hash, old_next);

            size_++;
            if (size_ >= length_) {
                Resize(length_ * 2);
            }
        }
    }

    V* Find(const K &key) {
        uint64_t hash = Hash(key);
        Node **ptr = FindPointer(key, hash);
        return (*ptr) ? &((*ptr)->value) : nullptr;
    }

    const V *Find(const K &key) const {
        uint64_t hash = Hash(key);
        Node *const *ptr = const_cast<HashTable *>(this)->FindPointer(key, hash);
        return (*ptr) ? &((*ptr)->value) : nullptr;
    }

    bool Erase(const K &key) {
        uint64_t hash = Hash(key);
        Node **ptr = FindPointer(key, hash);
        Node *target = *ptr;

        if (target != nullptr) {
            *ptr = target->next;
            delete target;
            size_--;
            return true;
        }
        return false;
    }

    void Clear() {
        if (!list_) {
            return;
        }
        for (uint64_t i = 0; i < length_; ++i) {
            Node *node = list_[i];
            while (node) {
                Node *next = node->next;
                delete node;
                node = next;
            }
            list_[i] = nullptr;
        }
        size_ = 0;
    }

    uint64_t Size() const {
        return size_;
    }

    uint64_t BucketCount() const {
        return length_;
    }

    template <typename Func>
    void ForEach(Func&& func) const {
        if (!list_) {
            return;
        }
        for (uint64_t i = 0; i < length_; ++i) {
            Node *node = list_[i];
            while (node) {
                func(node->key, node->value);
                node = node->next;
            }
        }
    }

private:
    Node **FindPointer(const K &key, uint64_t hash) const {
        Node **ptr = &list_[hash & (length_ - 1)];

        while (*ptr != nullptr &&
               ((*ptr)->hash != hash || (*ptr)->key != key)) {
            ptr = &((*ptr)->next);
        }
        return ptr;
    }

    void Resize(uint64_t new_length) {
        if (list_ == nullptr) {
            list_ = new Node *[new_length]();
            length_ = new_length;
        } else {
            Node **new_buckets = new Node *[new_length]();

            uint64_t count = 0;
            for (uint64_t i = 0; i < length_; ++i) {
                Node *h = list_[i];
                while (h != nullptr) {
                    Node *next = h->next;

                    uint32_t idx = h->hash & (new_length - 1);

                    h->next = new_buckets[idx];
                    new_buckets[idx] = h;

                    h = next;
                    count++;
                }
            }

            delete[] list_;
            list_ = new_buckets;
            length_ = new_length;
        }
    }

    uint64_t Hash(const K &key) const {
        return std::hash<K>{}(key);
    }

    Node **list_;
    uint64_t length_;
    uint64_t size_;
};
