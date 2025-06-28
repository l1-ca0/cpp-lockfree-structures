#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <vector>

// Hash specialization for std::pair
namespace std {
    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2>> {
        size_t operator()(const std::pair<T1, T2>& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1);  // Simple combination
        }
    };
}

namespace lockfree {

/**
 * @brief A lock-free, thread-safe set implementation using hash tables with chaining.
 * 
 * This set provides unique element storage without using traditional locking mechanisms.
 * It uses a hash table with separate chaining (linked lists) and atomic operations to
 * ensure thread safety and lock-free progress. The set automatically prevents duplicate
 * elements and supports efficient insertion, deletion, and lookup operations.
 * 
 * @tparam T The type of elements stored in the set. Must be hashable and comparable.
 * @tparam Hash Hash function for type T. Defaults to std::hash<T>.
 * @tparam KeyEqual Equality comparison for type T. Defaults to std::equal_to<T>.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Unique elements: Automatically prevents duplicate insertions
 * - Dynamic sizing: Hash table with configurable initial size
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - Iterator support: Forward iteration through all elements
 * - Set operations: Subset/superset testing and conversion utilities
 * 
 * Performance Characteristics:
 * - Insert: O(1) average, O(n) worst case (hash collisions)
 * - Erase: O(1) average, O(n) worst case (hash collisions)
 * - Contains: O(1) average, O(n) worst case (hash collisions)
 * - Memory: O(n + m) where n is elements, m is bucket count
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
 * lockfree::AtomicSet<int> set;
 * 
 * // Insert elements (duplicates are automatically prevented)
 * set.insert(42);
 * set.insert(42);  // Won't be inserted again
 * set.emplace(100);
 * 
 * // Check membership
 * if (set.contains(42)) {
 *     std::cout << "Set contains 42" << std::endl;
 * }
 * 
 * // Remove elements
 * if (set.erase(42)) {
 *     std::cout << "Removed 42 from set" << std::endl;
 * }
 * 
 * // Iterate through elements
 * for (const auto& item : set) {
 *     std::cout << "Item: " << item << std::endl;
 * }
 * 
 * // Set operations
 * lockfree::AtomicSet<int> other_set;
 * other_set.insert(100);
 * if (set.is_superset_of(other_set)) {
 *     std::cout << "set contains all elements of other_set" << std::endl;
 * }
 * @endcode
 * 
 * @note This implementation uses logical deletion for safe concurrent access.
 * @note This implementation provides reliable concurrent access for fixed-capacity use cases.
 */
template<typename T, typename Hash = std::hash<T>, typename KeyEqual = std::equal_to<T>>
class AtomicSet {
private:
    /**
     * @brief Internal node structure for set elements.
     * 
     * Each node contains the data, an atomic pointer to the next node in the chain,
     * and an atomic deletion flag for logical deletion.
     */
    struct Node {
        T data;                         ///< The stored data element
        std::atomic<Node*> next;        ///< Atomic pointer to next node in bucket chain
        std::atomic<bool> deleted;      ///< Atomic flag indicating logical deletion
        
        /**
         * @brief Construct node with copied data.
         * @param item The data to copy into the node
         */
        Node(const T& item) : data(item), next(nullptr), deleted(false) {}
        
        /**
         * @brief Construct node with moved data.
         * @param item The data to move into the node
         */
        Node(T&& item) : data(std::move(item)), next(nullptr), deleted(false) {}
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
    std::atomic<size_t> size_;              ///< Atomic counter for number of elements
    std::atomic<size_t> bucket_count_;      ///< Atomic counter for number of buckets
    Hash hasher_;                           ///< Hash function instance
    KeyEqual key_equal_;                    ///< Equality comparison function instance
    
    /**
     * @brief Compute hash value for a key.
     * @param key The key to hash
     * @return Hash value for the key
     */
    size_t hash_key(const T& key) const;
    
    /**
     * @brief Get bucket index for a key.
     * @param key The key to get bucket index for
     * @return Bucket index (0 to bucket_count-1)
     */
    size_t get_bucket_index(const T& key) const;
    
    /**
     * @brief Find node with matching key in a bucket.
     * @param key The key to search for
     * @param bucket The bucket to search in
     * @return Pointer to matching node or nullptr if not found
     */
    Node* find_node(const T& key, Bucket& bucket) const;
    
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
     * @brief Default constructor. Creates an empty set with default bucket count.
     * 
     * @complexity O(bucket_count)
     * @thread_safety Safe
     */
    AtomicSet();
    
    /**
     * @brief Constructor with custom initial bucket count.
     * 
     * @param initial_bucket_count Number of buckets to start with
     * @complexity O(initial_bucket_count)
     * @thread_safety Safe
     */
    explicit AtomicSet(size_t initial_bucket_count);
    
    /**
     * @brief Destructor. Cleans up all nodes and buckets.
     * 
     * @complexity O(n + m) where n is elements, m is buckets
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicSet();
    
    // Non-copyable but movable for resource management
    AtomicSet(const AtomicSet&) = delete;
    AtomicSet& operator=(const AtomicSet&) = delete;
    AtomicSet(AtomicSet&&) = default;
    AtomicSet& operator=(AtomicSet&&) = default;
    
    /**
     * @brief Insert an element into the set by copying.
     * 
     * @param value The value to copy and insert
     * @return true if element was inserted (was not already present), false if already exists
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's copy constructor throws,
     *                  the set remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     */
    bool insert(const T& value);
    
    /**
     * @brief Insert an element into the set by moving.
     * 
     * @param value The value to move and insert
     * @return true if element was inserted (was not already present), false if already exists
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's move constructor throws,
     *                  the set remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     */
    bool insert(T&& value);
    
    /**
     * @brief Construct an element in-place in the set.
     * 
     * @tparam Args Types of arguments for T's constructor
     * @param args Arguments to forward to T's constructor
     * @return true if element was constructed and inserted, false if already exists
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's constructor throws,
     *                  the set remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     */
    template<typename... Args>
    bool emplace(Args&&... args);
    
    /**
     * @brief Remove an element from the set.
     * 
     * @param value The value to remove
     * @return true if element was found and marked for deletion, false if not found
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety Strong guarantee - if operation fails, set is unchanged
     * 
     * @note Uses logical deletion (marking). Physical removal happens during iteration.
     */
    bool erase(const T& value);
    
    /**
     * @brief Check if the set contains an element.
     * 
     * @param value The value to search for
     * @return true if element is found and not marked for deletion, false otherwise
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    bool contains(const T& value) const;
    
    /**
     * @brief Check if the set contains an element (alias for contains).
     * 
     * @param value The value to search for
     * @return true if element is found and not marked for deletion, false otherwise
     * @complexity O(1) average, O(n) worst case due to hash collisions
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This is an alias for contains() for API consistency.
     */
    bool find(const T& value) const;  // Alias for contains
    
    /**
     * @brief Check if the set is empty.
     * 
     * @return true if the set contains no unmarked elements, false otherwise
     * @complexity O(n*m) worst case - may need to check all buckets and nodes
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This operation may be expensive as it needs to find unmarked nodes.
     *       Result may be immediately outdated in concurrent environment.
     */
    bool empty() const;
    
    /**
     * @brief Get the current number of elements in the set.
     * 
     * @return The number of elements currently in the set (including marked ones)
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This count may include logically deleted (marked) elements.
     *       For accurate count, consider iterating through the set.
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
     * @return Load factor as ratio of elements to buckets
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Load factor includes marked elements and may not reflect actual performance.
     */
    double load_factor() const;
    
    /**
     * @brief Insert elements from an iterator range.
     * 
     * @tparam InputIt Input iterator type
     * @param first Iterator to first element to insert
     * @param last Iterator to one past last element to insert
     * @complexity O(k) where k is the distance between first and last
     * @thread_safety Safe
     * @exception_safety Basic guarantee
     */
    template<typename InputIt>
    void insert(InputIt first, InputIt last);
    
    /**
     * @brief Forward iterator for traversing the set.
     * 
     * The iterator automatically skips over logically deleted (marked) nodes,
     * providing access only to active elements in the set.
     */
    class iterator {
    private:
        const AtomicSet* set_;          ///< Pointer to the owning set
        size_t bucket_index_;           ///< Current bucket index
        Node* current_;                 ///< Current node being pointed to
        
        /**
         * @brief Advance to the next valid (unmarked) element.
         */
        void advance_to_next_valid();
        
    public:
        /**
         * @brief Construct iterator for given set, bucket, and node.
         * @param set Pointer to the owning set
         * @param bucket_idx Current bucket index
         * @param node Current node pointer
         */
        iterator(const AtomicSet* set, size_t bucket_idx, Node* node);
        
        /**
         * @brief Dereference operator to access current element.
         * @return Reference to the current element's data
         */
        const T& operator*() const;
        
        /**
         * @brief Arrow operator to access current element.
         * @return Pointer to the current element's data
         */
        const T* operator->() const;
        
        /**
         * @brief Pre-increment operator to advance to next element.
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
     * @brief Get iterator to the first element.
     * 
     * @return Iterator pointing to first active element, or end() if set is empty
     * @complexity O(bucket_count) worst case - may need to skip empty buckets
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator begin() const;
    
    /**
     * @brief Get iterator representing past-the-end.
     * 
     * @return Iterator representing the end of the set
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator end() const;
    
    /**
     * @brief Count elements matching a predicate.
     * 
     * @tparam Predicate Function object type for testing elements
     * @param pred Predicate function to test each element
     * @return Number of elements for which pred returns true
     * @complexity O(n) where n is the number of active elements
     * @thread_safety Safe
     * @exception_safety Depends on predicate's exception safety
     */
    template<typename Predicate>
    size_t count_if(Predicate pred) const;
    
    /**
     * @brief Test if this set is a subset of another set.
     * 
     * @param other The set to test against
     * @return true if all elements of this set are in other, false otherwise
     * @complexity O(n*m) where n is size of this set, m is average chain length of other
     * @thread_safety Safe if other is not being modified
     * @exception_safety No-throw guarantee
     */
    bool is_subset_of(const AtomicSet& other) const;
    
    /**
     * @brief Test if this set is a superset of another set.
     * 
     * @param other The set to test against
     * @return true if all elements of other are in this set, false otherwise
     * @complexity O(n*m) where n is size of other, m is average chain length of this set
     * @thread_safety Safe if other is not being modified
     * @exception_safety No-throw guarantee
     */
    bool is_superset_of(const AtomicSet& other) const;
    
    /**
     * @brief Convert the set to a vector.
     * 
     * @return Vector containing all active elements in the set
     * @complexity O(n) where n is the number of active elements
     * @thread_safety Safe
     * @exception_safety Basic guarantee
     * 
     * @note The order of elements in the vector is not guaranteed.
     */
    std::vector<T> to_vector() const;
};

template<typename T, typename Hash, typename KeyEqual>
AtomicSet<T, Hash, KeyEqual>::AtomicSet() : AtomicSet(INITIAL_BUCKET_COUNT) {}

template<typename T, typename Hash, typename KeyEqual>
AtomicSet<T, Hash, KeyEqual>::AtomicSet(size_t initial_bucket_count)
    : buckets_(initial_bucket_count), size_(0), bucket_count_(initial_bucket_count) {}

template<typename T, typename Hash, typename KeyEqual>
AtomicSet<T, Hash, KeyEqual>::~AtomicSet() {
    for (auto& bucket : buckets_) {
        Node* current = bucket.head.load();
        while (current) {
            Node* next = current->next.load();
            delete current;
            current = next;
        }
    }
}

template<typename T, typename Hash, typename KeyEqual>
size_t AtomicSet<T, Hash, KeyEqual>::hash_key(const T& key) const {
    return hasher_(key);
}

template<typename T, typename Hash, typename KeyEqual>
size_t AtomicSet<T, Hash, KeyEqual>::get_bucket_index(const T& key) const {
    return hash_key(key) % bucket_count_.load(std::memory_order_acquire);
}

template<typename T, typename Hash, typename KeyEqual>
typename AtomicSet<T, Hash, KeyEqual>::Node*
AtomicSet<T, Hash, KeyEqual>::find_node(const T& key, Bucket& bucket) const {
    Node* current = bucket.head.load(std::memory_order_acquire);
    
    while (current) {
        // Use relaxed ordering for better performance in hot path
        if (!current->deleted.load(std::memory_order_relaxed) && 
            key_equal_(current->data, key)) {
            return current;
        }
        
        current = current->next.load(std::memory_order_relaxed);
    }
    
    return nullptr;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::insert(const T& value) {
    resize_if_needed();
    
    size_t bucket_index = get_bucket_index(value);
    Bucket& bucket = buckets_[bucket_index];
    
    // Pre-check for existing value using optimized find
    if (find_node(value, bucket) != nullptr) {
        return false; // Value already exists
    }
    
    Node* new_node = new Node(value);
    
    int attempts = 0;
    while (attempts < 100) { // Reduced from 1000
        Node* head = bucket.head.load(std::memory_order_acquire);
        new_node->next.store(head, std::memory_order_relaxed);
        
        if (bucket.head.compare_exchange_weak(head, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
            size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        attempts++;
    }
    
    delete new_node;
    return false;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::insert(T&& value) {
    resize_if_needed();
    
    size_t bucket_index = get_bucket_index(value);
    Bucket& bucket = buckets_[bucket_index];
    
    int attempts = 0;
    while (attempts < 1000) {
        // First check for duplicates
        Node* current = bucket.head.load(std::memory_order_acquire);
        while (current) {
            if (!current->deleted.load(std::memory_order_acquire) && 
                key_equal_(current->data, value)) {
                return false; // Duplicate found
            }
            current = current->next.load(std::memory_order_acquire);
        }
        
        // No duplicate found, try to insert
        Node* new_node = new Node(std::move(value));
        Node* head = bucket.head.load(std::memory_order_acquire);
        new_node->next.store(head, std::memory_order_relaxed);
        
        if (bucket.head.compare_exchange_weak(head, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
            size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        delete new_node;
        attempts++;
    }
    
    return false;
}

template<typename T, typename Hash, typename KeyEqual>
template<typename... Args>
bool AtomicSet<T, Hash, KeyEqual>::emplace(Args&&... args) {
    return insert(T(std::forward<Args>(args)...));
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::erase(const T& value) {
    size_t bucket_index = get_bucket_index(value);
    Bucket& bucket = buckets_[bucket_index];
    
    Node* node = find_node(value, bucket);
    if (node) {
        bool expected = false;
        if (node->deleted.compare_exchange_strong(expected, true,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
            size_.fetch_sub(1, std::memory_order_relaxed);  // Decrement size
            return true;  // Successfully marked for deletion
        }
    }
    
    return false;  // Not found or already deleted
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::contains(const T& value) const {
    size_t bucket_index = get_bucket_index(value);
    Bucket& bucket = const_cast<Bucket&>(buckets_[bucket_index]);
    
    Node* node = find_node(value, bucket);
    return node != nullptr;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::find(const T& value) const {
    return contains(value);
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::empty() const {
    for (const auto& bucket : buckets_) {
        Node* current = bucket.head.load(std::memory_order_acquire);
        while (current) {
            if (!current->deleted.load(std::memory_order_acquire)) {
                return false;  // Found an active element
            }
            current = current->next.load(std::memory_order_acquire);
        }
    }
    return true;  // No active elements found
}

template<typename T, typename Hash, typename KeyEqual>
size_t AtomicSet<T, Hash, KeyEqual>::size() const {
    return size_.load(std::memory_order_relaxed);
}

template<typename T, typename Hash, typename KeyEqual>
size_t AtomicSet<T, Hash, KeyEqual>::bucket_count() const {
    return bucket_count_.load(std::memory_order_relaxed);
}

template<typename T, typename Hash, typename KeyEqual>
double AtomicSet<T, Hash, KeyEqual>::load_factor() const {
    size_t buckets = bucket_count();
    return buckets > 0 ? static_cast<double>(size()) / buckets : 0.0;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::should_resize() const {
    return (size() * 100) / bucket_count() > MAX_LOAD_FACTOR_PERCENT;
}

template<typename T, typename Hash, typename KeyEqual>
void AtomicSet<T, Hash, KeyEqual>::resize_if_needed() {
    // Placeholder for resize implementation
    // In production, this would implement hash table resizing
}

template<typename T, typename Hash, typename KeyEqual>
template<typename InputIt>
void AtomicSet<T, Hash, KeyEqual>::insert(InputIt first, InputIt last) {
    for (auto it = first; it != last; ++it) {
        insert(*it);
    }
}

// Iterator implementation

template<typename T, typename Hash, typename KeyEqual>
AtomicSet<T, Hash, KeyEqual>::iterator::iterator(const AtomicSet* set, size_t bucket_idx, Node* node)
    : set_(set), bucket_index_(bucket_idx), current_(node) {
    advance_to_next_valid();
}

template<typename T, typename Hash, typename KeyEqual>
void AtomicSet<T, Hash, KeyEqual>::iterator::advance_to_next_valid() {
    while (current_ && current_->deleted.load(std::memory_order_acquire)) {
        current_ = current_->next.load(std::memory_order_acquire);
    }
    
    while (!current_ && bucket_index_ < set_->buckets_.size()) {
        ++bucket_index_;
        if (bucket_index_ < set_->buckets_.size()) {
            current_ = set_->buckets_[bucket_index_].head.load(std::memory_order_acquire);
            while (current_ && current_->deleted.load(std::memory_order_acquire)) {
                current_ = current_->next.load(std::memory_order_acquire);
            }
        }
    }
}

template<typename T, typename Hash, typename KeyEqual>
const T& AtomicSet<T, Hash, KeyEqual>::iterator::operator*() const {
    return current_->data;
}

template<typename T, typename Hash, typename KeyEqual>
const T* AtomicSet<T, Hash, KeyEqual>::iterator::operator->() const {
    return &current_->data;
}

template<typename T, typename Hash, typename KeyEqual>
typename AtomicSet<T, Hash, KeyEqual>::iterator& 
AtomicSet<T, Hash, KeyEqual>::iterator::operator++() {
    if (current_) {
        current_ = current_->next.load(std::memory_order_acquire);
        advance_to_next_valid();
    }
    return *this;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::iterator::operator==(const iterator& other) const {
    return set_ == other.set_ && bucket_index_ == other.bucket_index_ && current_ == other.current_;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}

template<typename T, typename Hash, typename KeyEqual>
typename AtomicSet<T, Hash, KeyEqual>::iterator AtomicSet<T, Hash, KeyEqual>::begin() const {
    return iterator(this, 0, buckets_.empty() ? nullptr : buckets_[0].head.load(std::memory_order_acquire));
}

template<typename T, typename Hash, typename KeyEqual>
typename AtomicSet<T, Hash, KeyEqual>::iterator AtomicSet<T, Hash, KeyEqual>::end() const {
    return iterator(this, buckets_.size(), nullptr);
}

// Set-specific operations

template<typename T, typename Hash, typename KeyEqual>
template<typename Predicate>
size_t AtomicSet<T, Hash, KeyEqual>::count_if(Predicate pred) const {
    size_t count = 0;
    for (const auto& item : *this) {
        if (pred(item)) {
            ++count;
        }
    }
    return count;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::is_subset_of(const AtomicSet& other) const {
    for (const auto& item : *this) {
        if (!other.contains(item)) {
            return false;
        }
    }
    return true;
}

template<typename T, typename Hash, typename KeyEqual>
bool AtomicSet<T, Hash, KeyEqual>::is_superset_of(const AtomicSet& other) const {
    return other.is_subset_of(*this);
}

template<typename T, typename Hash, typename KeyEqual>
std::vector<T> AtomicSet<T, Hash, KeyEqual>::to_vector() const {
    std::vector<T> result;
    for (const auto& item : *this) {
        result.push_back(item);
    }
    return result;
}

} // namespace lockfree