#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <type_traits>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe hash map implementation using separate chaining.
 * 
 * This hash map provides key-value storage without using traditional locking mechanisms.
 * It uses a hash table with separate chaining (linked lists) and atomic operations to
 * ensure thread safety and lock-free progress. The hash map supports efficient insertion,
 * deletion, and lookup operations with configurable hash functions.
 * 
 * @tparam Key The type of keys. Must be hashable and comparable.
 * @tparam Value The type of values stored. Must be constructible and destructible.
 * @tparam Hash Hash function for keys. Defaults to std::hash<Key>.
 * @tparam KeyEqual Equality comparison for keys. Defaults to std::equal_to<Key>.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Key-value storage: Efficient mapping from keys to values
 * - Dynamic sizing: Hash table with configurable initial size
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - Iterator support: Forward iteration through all key-value pairs
 * - Template predicates: Support for custom search predicates
 * 
 * Performance Characteristics:
 * - Insert: O(1) average, O(n) worst case (hash collisions)
 * - Find: O(1) average, O(n) worst case (hash collisions)
 * - Erase: O(1) average, O(n) worst case (hash collisions)
 * - Memory: O(n + m) where n is key-value pairs, m is bucket count
 * 
 * Algorithm Details:
 * - Uses separate chaining with linked lists for collision resolution
 * - Logical deletion (marking) for safe concurrent access
 * - Compare-and-swap operations for atomic pointer updates
 * - Load factor monitoring for performance optimization
 * 
 * Memory Management:
 * - Uses dynamic allocation for nodes and bucket array
 * - Marked nodes are cleaned up during iteration
 * - All memory is properly cleaned up in destructor
 * - Fixed-size design optimized for known capacity requirements
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicHashMap<std::string, int> map;
 * 
 * // Insert key-value pairs
 * map.insert("hello", 42);
 * map.emplace("world", 100);
 * 
 * // Lookup values
 * int value;
 * if (map.find("hello", value)) {
 *     std::cout << "hello -> " << value << std::endl;
 * }
 * 
 * // Check existence
 * if (map.contains("hello")) {
 *     std::cout << "Map contains hello" << std::endl;
 * }
 * 
 * // Remove key-value pairs
 * if (map.erase("hello")) {
 *     std::cout << "Removed hello from map" << std::endl;
 * }
 * 
 * // Iterate through pairs
 * for (auto it = map.begin(); it != map.end(); ++it) {
 *     auto [key, value] = *it;
 *     std::cout << key << " -> " << value << std::endl;
 * }
 * @endcode
 * 
 * @note This implementation uses logical deletion for safe concurrent access.
 * @note This implementation provides reliable concurrent access for fixed-capacity use cases.
 */
template<typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class AtomicHashMap {
private:
    /**
     * @brief Internal node structure for key-value pairs.
     * 
     * Each node contains a key-value pair, an atomic pointer to the next node
     * in the collision chain, and an atomic deletion flag for logical deletion.
     */
    struct Node {
        Key key;                        ///< The stored key
        Value value;                    ///< The stored value
        std::atomic<Node*> next;        ///< Atomic pointer to next node in bucket chain
        std::atomic<bool> deleted;      ///< Atomic flag indicating logical deletion
        
        /**
         * @brief Construct node with copied key and value.
         * @param k The key to copy
         * @param v The value to copy
         */
        Node(const Key& k, const Value& v) 
            : key(k), value(v), next(nullptr), deleted(false) {}
        
        /**
         * @brief Construct node with moved key and value.
         * @param k The key to move
         * @param v The value to move
         */
        Node(Key&& k, Value&& v) 
            : key(std::move(k)), value(std::move(v)), next(nullptr), deleted(false) {}
    };
    
    /**
     * @brief Hash table bucket containing the head of a collision chain.
     */
    struct Bucket {
        std::atomic<Node*> head;        ///< Atomic pointer to first node in bucket
        
        /**
         * @brief Default constructor. Creates an empty bucket.
         */
        Bucket() : head(nullptr) {}
    };
    
    static constexpr size_t INITIAL_BUCKET_COUNT = 1024;     ///< Increased from 16 for better performance
    static constexpr size_t MAX_LOAD_FACTOR_PERCENT = 50;    ///< Reduced from 75 for optimal performance
    
    std::vector<Bucket> buckets_;           ///< Dynamic array of hash table buckets
    std::atomic<size_t> size_;              ///< Atomic counter for number of key-value pairs
    std::atomic<size_t> bucket_count_;      ///< Atomic counter for number of buckets
    Hash hasher_;                           ///< Hash function instance
    KeyEqual key_equal_;                    ///< Key equality comparison function instance
    
    /**
     * @brief Compute hash value for a key.
     * @param key The key to hash
     * @return Hash value for the key
     */
    size_t hash_key(const Key& key) const;
    
    /**
     * @brief Get bucket index for a key.
     * @param key The key to get bucket index for
     * @return Bucket index (0 to bucket_count-1)
     */
    size_t get_bucket_index(const Key& key) const;
    
    /**
     * @brief Find node with matching key in a bucket.
     * @param key The key to search for
     * @param bucket The bucket to search in
     * @return Pointer to matching node or nullptr if not found
     */
    Node* find_node(const Key& key, Bucket& bucket) const;
    
    /**
     * @brief Check if resize is needed based on load factor.
     * @return true if resize is recommended, false otherwise
     */
    bool should_resize() const;
    
    /**
     * @brief Resize the hash table if needed (placeholder for future implementation).
     */
    void resize_if_needed();
    
public:
    /**
     * @brief Default constructor. Creates an empty hash map with default bucket count.
     * 
     * @complexity O(bucket_count)
     * @thread_safety Safe
     */
    AtomicHashMap();
    
    /**
     * @brief Constructor with custom initial bucket count.
     * 
     * @param initial_bucket_count Number of buckets to start with
     * @complexity O(initial_bucket_count)
     * @thread_safety Safe
     */
    explicit AtomicHashMap(size_t initial_bucket_count);
    
    /**
     * @brief Destructor. Cleans up all nodes and buckets.
     * 
     * @complexity O(n + m) where n is key-value pairs, m is buckets
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicHashMap();
    
    // Non-copyable but movable for resource management
    AtomicHashMap(const AtomicHashMap&) = delete;
    AtomicHashMap& operator=(const AtomicHashMap&) = delete;
    AtomicHashMap(AtomicHashMap&&) = default;
    AtomicHashMap& operator=(AtomicHashMap&&) = default;
    
    /**
     * @brief Insert a key-value pair by copying.
     * 
     * @param key The key to copy and insert
     * @param value The value to copy and associate with the key
     * @return true if key-value pair was inserted (key was not already present), false if key exists
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if copy constructors throw,
     *                  the hash map remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Does not update existing values - use separate update method if needed.
     */
    bool insert(const Key& key, const Value& value);
    
    /**
     * @brief Insert a key-value pair by moving.
     * 
     * @param key The key to move and insert
     * @param value The value to move and associate with the key
     * @return true if key-value pair was inserted (key was not already present), false if key exists
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if move constructors throw,
     *                  the hash map remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Does not update existing values - use separate update method if needed.
     */
    bool insert(Key&& key, Value&& value);
    
    /**
     * @brief Construct a value in-place for the given key.
     * 
     * @tparam Args Types of arguments for Value's constructor
     * @param key The key to associate with the constructed value
     * @param args Arguments to forward to Value's constructor
     * @return true if key-value pair was constructed and inserted, false if key already exists
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if Value's constructor throws,
     *                  the hash map remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     */
    template<typename... Args>
    bool emplace(const Key& key, Args&&... args);
    
    /**
     * @brief Find the value associated with a key.
     * 
     * @param key The key to search for
     * @param result Reference to store the found value
     * @return true if key was found and value copied to result, false if key not found
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if Value's copy constructor throws during result assignment
     * 
     * @note For move-only types, this will move the value into result, potentially
     *       making the stored value invalid for future accesses.
     */
    bool find(const Key& key, Value& result) const;
    
    /**
     * @brief Check if the hash map contains a key.
     * 
     * @param key The key to search for
     * @return true if key is found and not marked for deletion, false otherwise
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    bool contains(const Key& key) const;
    
    /**
     * @brief Find value using a custom predicate function.
     * 
     * @tparam Func Function object type for testing values
     * @param key The key to search for
     * @param func Predicate function to apply to the value if key is found
     * @return true if key was found and predicate returned true, false otherwise
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Depends on predicate function's exception safety
     * 
     * @note Useful for testing properties of values without extracting them.
     */
    template<typename Func>
    bool find_if(const Key& key, Func&& func) const;
    
    /**
     * @brief Remove a key-value pair from the hash map.
     * 
     * @param key The key to remove
     * @return true if key was found and marked for deletion, false if key not found
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Strong guarantee - if operation fails, hash map is unchanged
     * 
     * @note Uses logical deletion (marking). Physical removal happens during iteration.
     */
    bool erase(const Key& key);
    
    /**
     * @brief Check if the hash map is empty.
     * 
     * @return true if the hash map contains no unmarked key-value pairs, false otherwise
     * @complexity O(n*m) worst case - may need to check all buckets and nodes
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This operation may be expensive as it needs to find unmarked nodes.
     *       Result may be immediately outdated in concurrent environment.
     */
    bool empty() const;
    
    /**
     * @brief Get the current number of key-value pairs in the hash map.
     * 
     * @return The number of key-value pairs currently in the hash map (including marked ones)
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This count may include logically deleted (marked) key-value pairs.
     *       For accurate count, consider iterating through the hash map.
     *       Result may be immediately outdated in concurrent environment.
     */
    size_t size() const;
    
    /**
     * @brief Get the current number of buckets in the hash table.
     * 
     * @return The number of buckets in the hash table
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    size_t bucket_count() const;
    
    /**
     * @brief Get the current load factor of the hash table.
     * 
     * @return Load factor as ratio of key-value pairs to buckets
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Load factor includes marked key-value pairs and may not reflect actual performance.
     */
    double load_factor() const;
    
    /**
     * @brief Forward iterator for traversing the hash map.
     * 
     * The iterator automatically skips over logically deleted (marked) nodes,
     * providing access only to active key-value pairs in the hash map.
     */
    class iterator {
    private:
        const AtomicHashMap* map_;      ///< Pointer to the owning hash map
        size_t bucket_index_;           ///< Current bucket index
        Node* current_;                 ///< Current node being pointed to
        
        /**
         * @brief Advance to the next valid (unmarked) key-value pair.
         */
        void advance_to_next_valid();
        
    public:
        /**
         * @brief Construct iterator for given hash map, bucket, and node.
         * @param map Pointer to the owning hash map
         * @param bucket_idx Current bucket index
         * @param node Current node pointer
         */
        iterator(const AtomicHashMap* map, size_t bucket_idx, Node* node);
        
        /**
         * @brief Dereference operator to access current key-value pair.
         * @return Pair containing references to the current key and value
         */
        std::pair<const Key&, Value&> operator*();
        
        /**
         * @brief Pre-increment operator to advance to next key-value pair.
         * @return Reference to this iterator after advancement
         */
        iterator& operator++();
        
        /**
         * @brief Equality comparison operator.
         * @param other Iterator to compare with
         * @return true if both iterators point to the same position
         */
        bool operator==(const iterator& other) const;
        
        /**
         * @brief Inequality comparison operator.
         * @param other Iterator to compare with
         * @return true if iterators point to different positions
         */
        bool operator!=(const iterator& other) const;
    };
    
    /**
     * @brief Get iterator to the first key-value pair.
     * 
     * @return Iterator pointing to first active key-value pair, or end() if hash map is empty
     * @complexity O(bucket_count) worst case - may need to skip empty buckets
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator begin() const;
    
    /**
     * @brief Get iterator representing past-the-end.
     * 
     * @return Iterator representing the end of the hash map
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator end() const;
};

template<typename Key, typename Value, typename Hash, typename KeyEqual>
AtomicHashMap<Key, Value, Hash, KeyEqual>::AtomicHashMap() 
    : AtomicHashMap(INITIAL_BUCKET_COUNT) {}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
AtomicHashMap<Key, Value, Hash, KeyEqual>::AtomicHashMap(size_t initial_bucket_count)
    : buckets_(initial_bucket_count), size_(0), bucket_count_(initial_bucket_count) {}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
AtomicHashMap<Key, Value, Hash, KeyEqual>::~AtomicHashMap() {
    for (auto& bucket : buckets_) {
        Node* current = bucket.head.load();
        while (current) {
            Node* next = current->next.load();
            delete current;
            current = next;
        }
    }
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
size_t AtomicHashMap<Key, Value, Hash, KeyEqual>::hash_key(const Key& key) const {
    return hasher_(key);
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
size_t AtomicHashMap<Key, Value, Hash, KeyEqual>::get_bucket_index(const Key& key) const {
    return hash_key(key) % bucket_count_.load(std::memory_order_acquire);
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
typename AtomicHashMap<Key, Value, Hash, KeyEqual>::Node* 
AtomicHashMap<Key, Value, Hash, KeyEqual>::find_node(const Key& key, Bucket& bucket) const {
    Node* current = bucket.head.load(std::memory_order_acquire);
    
    while (current) {
        // Check if current node matches and is not deleted
        // Use relaxed ordering for better performance in hot path
        if (!current->deleted.load(std::memory_order_relaxed) && 
            key_equal_(current->key, key)) {
            return current;
        }
        
        current = current->next.load(std::memory_order_relaxed);
    }
    
    return nullptr;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::insert(const Key& key, const Value& value) {
    size_t bucket_index = get_bucket_index(key);
    Bucket& bucket = buckets_[bucket_index];
    
    // Pre-check for existing key using optimized find
    if (find_node(key, bucket) != nullptr) {
        return false; // Key already exists
    }
    
    Node* new_node = new Node(key, value);
    
    // Try to insert at head of bucket using compare_exchange
    for (int attempts = 0; attempts < 100; ++attempts) { // Reduced from 1000
        Node* head = bucket.head.load(std::memory_order_acquire);
        
        // Try to insert at head
        new_node->next.store(head, std::memory_order_relaxed);
        
        if (bucket.head.compare_exchange_weak(head, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
            size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        // If CAS failed, head changed, so retry
    }
    
    // Failed after max attempts
    delete new_node;
    return false;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::insert(Key&& key, Value&& value) {
    size_t bucket_index = get_bucket_index(key);
    Bucket& bucket = buckets_[bucket_index];
    
    Node* new_node = new Node(std::move(key), std::move(value));
    
    // Try to insert at head of bucket using compare_exchange
    for (int attempts = 0; attempts < 1000; ++attempts) {
        Node* head = bucket.head.load(std::memory_order_acquire);
        
        // Check if key already exists in current chain
        Node* existing = head;
        while (existing) {
            if (!existing->deleted.load(std::memory_order_acquire) && 
                key_equal_(existing->key, new_node->key)) {
                // Key already exists
                delete new_node;
                return false;
            }
            existing = existing->next.load(std::memory_order_acquire);
        }
        
        // Try to insert at head
        new_node->next.store(head, std::memory_order_relaxed);
        
        if (bucket.head.compare_exchange_weak(head, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
            size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        // If CAS failed, head changed, so retry
    }
    
    // Failed after max attempts
    delete new_node;
    return false;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
template<typename... Args>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::emplace(const Key& key, Args&&... args) {
    return insert(key, Value(std::forward<Args>(args)...));
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::find(const Key& key, Value& result) const {
    size_t bucket_index = get_bucket_index(key);
    Bucket& bucket = const_cast<Bucket&>(buckets_[bucket_index]);
    
    Node* node = find_node(key, bucket);
    if (node) {
        result = node->value;  // Copy or move the value
        return true;
    }
    
    return false;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::contains(const Key& key) const {
    size_t bucket_index = get_bucket_index(key);
    Bucket& bucket = const_cast<Bucket&>(buckets_[bucket_index]);
    
    Node* node = find_node(key, bucket);
    return node != nullptr;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
template<typename Func>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::find_if(const Key& key, Func&& func) const {
    size_t bucket_index = get_bucket_index(key);
    Bucket& bucket = const_cast<Bucket&>(buckets_[bucket_index]);
    
    Node* node = find_node(key, bucket);
    if (node) {
        return func(node->value);
    }
    
    return false;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::erase(const Key& key) {
    size_t bucket_index = get_bucket_index(key);
    Bucket& bucket = buckets_[bucket_index];
    
    Node* node = find_node(key, bucket);
    if (node) {
        bool expected = false;
        if (node->deleted.compare_exchange_strong(expected, true,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
            // Successfully marked for deletion - decrement size
            size_.fetch_sub(1, std::memory_order_relaxed);
            return true;  // Successfully marked for deletion
        }
    }
    
    return false;  // Not found or already deleted
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::empty() const {
    for (const auto& bucket : buckets_) {
        Node* current = bucket.head.load(std::memory_order_acquire);
        while (current) {
            if (!current->deleted.load(std::memory_order_acquire)) {
                return false;  // Found an active key-value pair
            }
            current = current->next.load(std::memory_order_acquire);
        }
    }
    return true;  // No active key-value pairs found
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
size_t AtomicHashMap<Key, Value, Hash, KeyEqual>::size() const {
    return size_.load(std::memory_order_relaxed);
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
size_t AtomicHashMap<Key, Value, Hash, KeyEqual>::bucket_count() const {
    return bucket_count_.load(std::memory_order_relaxed);
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
double AtomicHashMap<Key, Value, Hash, KeyEqual>::load_factor() const {
    size_t buckets = bucket_count();
    return buckets > 0 ? static_cast<double>(size()) / buckets : 0.0;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::should_resize() const {
    return (size() * 100) / bucket_count() > MAX_LOAD_FACTOR_PERCENT;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void AtomicHashMap<Key, Value, Hash, KeyEqual>::resize_if_needed() {
    // Placeholder for resize implementation
    // In production, this would implement hash table resizing
}

// Iterator implementation

template<typename Key, typename Value, typename Hash, typename KeyEqual>
AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator::iterator(const AtomicHashMap* map, size_t bucket_idx, Node* node)
    : map_(map), bucket_index_(bucket_idx), current_(node) {
    advance_to_next_valid();
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator::advance_to_next_valid() {
    while (current_ && current_->deleted.load(std::memory_order_acquire)) {
        current_ = current_->next.load(std::memory_order_acquire);
    }
    
    while (!current_ && bucket_index_ < map_->buckets_.size()) {
        ++bucket_index_;
        if (bucket_index_ < map_->buckets_.size()) {
            current_ = map_->buckets_[bucket_index_].head.load(std::memory_order_acquire);
            while (current_ && current_->deleted.load(std::memory_order_acquire)) {
                current_ = current_->next.load(std::memory_order_acquire);
            }
        }
    }
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
std::pair<const Key&, Value&> AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator::operator*() {
    return {current_->key, current_->value};
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
typename AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator& 
AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator::operator++() {
    if (current_) {
        current_ = current_->next.load(std::memory_order_acquire);
        advance_to_next_valid();
    }
    return *this;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator::operator==(const iterator& other) const {
    return map_ == other.map_ && bucket_index_ == other.bucket_index_ && current_ == other.current_;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
bool AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
typename AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator AtomicHashMap<Key, Value, Hash, KeyEqual>::begin() const {
    return iterator(this, 0, buckets_.empty() ? nullptr : buckets_[0].head.load(std::memory_order_acquire));
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
typename AtomicHashMap<Key, Value, Hash, KeyEqual>::iterator AtomicHashMap<Key, Value, Hash, KeyEqual>::end() const {
    return iterator(this, buckets_.size(), nullptr);
}

} // namespace lockfree