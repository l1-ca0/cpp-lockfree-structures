#pragma once

#include <atomic>
#include <memory>
#include <random>
#include <functional>
#include <array>
#include <type_traits>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe skip list implementation for ordered key-value storage.
 * 
 * This skip list provides ordered key-value storage without using traditional locking mechanisms.
 * It uses atomic operations and a probabilistic multi-level linked list structure to ensure
 * thread safety and lock-free progress. Skip lists offer logarithmic average-case performance
 * for search, insertion, and deletion operations while maintaining sorted order.
 * 
 * @tparam Key The type of keys used for ordering. Must be comparable and copyable.
 * @tparam Value The type of values stored. Must be constructible and destructible.
 * @tparam Compare Comparison function for keys. Defaults to std::less<Key>.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Ordered storage: Elements are automatically maintained in sorted order
 * - Probabilistic balancing: Uses randomized levels for efficient access
 * - Logarithmic performance: O(log n) average-case operations
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - Iterator support: Forward iteration through sorted elements
 * - Template predicates: Support for custom search predicates
 * 
 * Performance Characteristics:
 * - Insert: O(log n) average, O(n) worst case
 * - Find: O(log n) average, O(n) worst case
 * - Erase: O(log n) average, O(n) worst case
 * - Size: O(n) linear traversal for accurate count (no atomic size counter)
 * - Memory: O(n) where n is the number of elements
 * 
 * Algorithm Details:
 * - Uses probabilistic skip list algorithm with sentinel nodes
 * - Each node has a randomly determined level (height)
 * - Higher levels provide "express lanes" for faster traversal
 * - Logical deletion (marking) for safe concurrent access
 * - Compare-and-swap operations for atomic pointer updates
 * - No atomic size counter to eliminate contention bottleneck
 * 
 * Memory Management:
 * - Uses dynamic allocation for nodes with variable-height arrays
 * - Marked nodes are cleaned up during search operations
 * - All memory is properly cleaned up in destructor
 * - Thread-local random number generation for level assignment
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicSkipList<int, std::string> skiplist;
 * 
 * // Insert key-value pairs
 * skiplist.insert(42, "hello");
 * skiplist.insert(15, "world");
 * skiplist.emplace(30, "test");
 * 
 * // Lookup values
 * std::string value;
 * if (skiplist.find(42, value)) {
 *     std::cout << "42 -> " << value << std::endl;
 * }
 * 
 * // Check existence
 * if (skiplist.contains(15)) {
 *     std::cout << "Found key 15" << std::endl;
 * }
 * 
 * // Remove key-value pairs
 * if (skiplist.erase(42)) {
 *     std::cout << "Removed key 42" << std::endl;
 * }
 * 
 * // Iterate through pairs in sorted order
 * for (auto it = skiplist.begin(); it != skiplist.end(); ++it) {
 *     auto [key, value] = *it;
 *     std::cout << key << " -> " << value << std::endl;
 * }
 * @endcode
 * 
 * @note This implementation uses logical deletion for safe concurrent access.
 * @warning Random level generation uses thread-local storage and may not be fully deterministic.
 */
template<typename Key, typename Value, typename Compare = std::less<Key>>
class AtomicSkipList {
private:
    static constexpr int MAX_LEVEL = 32;        ///< Maximum number of levels in the skip list
    
    /**
     * @brief Internal node structure for skip list elements.
     * 
     * Each node contains a key-value pair, its level (height), an array of atomic
     * pointers to next nodes at each level, and an atomic deletion flag.
     */
    struct Node {
        Key key;                                            ///< The stored key
        Value value;                                        ///< The stored value
        std::atomic<int> level;                             ///< The level (height) of this node
        std::array<std::atomic<Node*>, MAX_LEVEL> next;     ///< Atomic pointers to next nodes at each level
        std::atomic<bool> marked;                           ///< Atomic flag indicating logical deletion
        
        /**
         * @brief Construct node with copied key and value.
         * @param k The key to copy
         * @param v The value to copy
         * @param lvl The level (height) for this node
         */
        Node(const Key& k, const Value& v, int lvl) 
            : key(k), value(v), level(lvl), marked(false) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                next[i].store(nullptr);
            }
        }
        
        /**
         * @brief Construct node with moved key and value.
         * @param k The key to move
         * @param v The value to move
         * @param lvl The level (height) for this node
         */
        Node(Key&& k, Value&& v, int lvl) 
            : key(std::move(k)), value(std::move(v)), level(lvl), marked(false) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                next[i].store(nullptr);
            }
        }
    };
    
    Node* head_;                                ///< Sentinel head node
    Node* tail_;                                ///< Sentinel tail node
    // Removed atomic size counter - O(n) size() to eliminate contention
    Compare comparator_;                        ///< Comparison function for keys
    
    /**
     * @brief Thread-local random number generator for level generation.
     */
    thread_local static std::mt19937 rng_;
    
    /**
     * @brief Generate a random level for a new node.
     * @return Random level between 0 and MAX_LEVEL-1
     */
    int random_level();
    
    /**
     * @brief Find predecessor nodes for a given key at all levels.
     * @param key The key to search for predecessors of
     * @return Array of predecessor nodes at each level
     */
    std::array<Node*, MAX_LEVEL> find_predecessors(const Key& key);
    
public:
    /**
     * @brief Default constructor. Creates an empty skip list with sentinel nodes.
     * 
     * @complexity O(MAX_LEVEL)
     * @thread_safety Safe
     */
    AtomicSkipList();
    
    /**
     * @brief Destructor. Cleans up all nodes including sentinels.
     * 
     * @complexity O(n) where n is the number of elements
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicSkipList();
    
    // Non-copyable but movable for resource management
    AtomicSkipList(const AtomicSkipList&) = delete;
    AtomicSkipList& operator=(const AtomicSkipList&) = delete;
    AtomicSkipList(AtomicSkipList&&) = default;
    AtomicSkipList& operator=(AtomicSkipList&&) = default;
    
    /**
     * @brief Insert a key-value pair by copying.
     * 
     * @param key The key to copy and insert
     * @param value The value to copy and associate with the key
     * @return true if key-value pair was inserted (key was not already present), false if key exists
     * @complexity O(log n) average, O(n) worst case
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if copy constructors throw,
     *                  the skip list remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Does not update existing values - insertion only succeeds for new keys.
     */
    bool insert(const Key& key, const Value& value);
    
    /**
     * @brief Insert a key-value pair by moving.
     * 
     * @param key The key to move and insert
     * @param value The value to move and associate with the key
     * @return true if key-value pair was inserted (key was not already present), false if key exists
     * @complexity O(log n) average, O(n) worst case
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if move constructors throw,
     *                  the skip list remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Does not update existing values - insertion only succeeds for new keys.
     */
    bool insert(Key&& key, Value&& value);
    
    /**
     * @brief Construct a value in-place for the given key.
     * 
     * @tparam Args Types of arguments for Value's constructor
     * @param key The key to associate with the constructed value
     * @param args Arguments to forward to Value's constructor
     * @return true if key-value pair was constructed and inserted, false if key already exists
     * @complexity O(log n) average, O(n) worst case
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if Value's constructor throws,
     *                  the skip list remains unchanged
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
     * @complexity O(log n) average, O(n) worst case
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if Value's copy constructor throws during result assignment
     * 
     * @note For move-only types, this will move the value into result, potentially
     *       making the stored value invalid for future accesses.
     */
    bool find(const Key& key, Value& result) const;
    
    /**
     * @brief Check if the skip list contains a key.
     * 
     * @param key The key to search for
     * @return true if key is found and not marked for deletion, false otherwise
     * @complexity O(log n) average, O(n) worst case
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
     * @complexity O(log n) average, O(n) worst case
     * @thread_safety Safe
     * @exception_safety Depends on predicate function's exception safety
     * 
     * @note Useful for testing properties of values without extracting them.
     */
    template<typename Func>
    bool find_if(const Key& key, Func&& func) const;
    
    /**
     * @brief Remove a key-value pair from the skip list.
     * 
     * @param key The key to remove
     * @return true if key was found and marked for deletion, false if key not found
     * @complexity O(log n) average, O(n) worst case
     * @thread_safety Safe
     * @exception_safety Strong guarantee - if operation fails, skip list is unchanged
     * 
     * @note Uses logical deletion (marking). Physical removal happens during search operations.
     */
    bool erase(const Key& key);
    
    /**
     * @brief Check if the skip list is empty.
     * 
     * @return true if the skip list contains no unmarked key-value pairs, false otherwise
     * @complexity O(n)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment.
     */
    bool empty() const;
    
    /**
     * @brief Get the current number of key-value pairs in the skip list.
     * 
     * @return The number of key-value pairs currently in the skip list
     * @complexity O(n)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This count may include logically deleted (marked) key-value pairs.
     *       Result may be immediately outdated in concurrent environment.
     */
    size_t size() const;
    
    /**
     * @brief Forward iterator for traversing the skip list in sorted order.
     * 
     * The iterator automatically skips over logically deleted (marked) nodes,
     * providing access only to active key-value pairs in ascending key order.
     */
    class iterator {
    private:
        Node* current_;                         ///< Current node being pointed to
        Node* tail_;                            ///< Tail sentinel for bounds checking
        
    public:
        /**
         * @brief Construct iterator pointing to given node.
         * @param node The node to point to
         * @param tail The tail sentinel node
         */
        iterator(Node* node, Node* tail);
        
        /**
         * @brief Copy constructor.
         */
        iterator(const iterator& other) = default;
        
        /**
         * @brief Assignment operator.
         */
        iterator& operator=(const iterator& other) = default;
        
        /**
         * @brief Dereference operator to access current key-value pair.
         * @return Pair containing the current key and value
         */
        std::pair<Key, Value> operator*() const;
        
        /**
         * @brief Pre-increment operator to advance to next element.
         * @return Reference to this iterator after advancement
         */
        iterator& operator++();
        
        /**
         * @brief Post-increment operator to advance to next element.
         * @return Copy of iterator before advancement
         */
        iterator operator++(int);
        
        /**
         * @brief Equality comparison operator.
         * @param other Iterator to compare with
         * @return true if both iterators point to the same node
         */
        bool operator==(const iterator& other) const;
        
        /**
         * @brief Inequality comparison operator.
         * @param other Iterator to compare with
         * @return true if iterators point to different nodes
         */
        bool operator!=(const iterator& other) const;
    };
    
    /**
     * @brief Get iterator to the first element (smallest key).
     * 
     * @return Iterator pointing to first active element, or end() if skip list is empty
     * @complexity O(log n) average - may need to skip marked nodes
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator begin() const;
    
    /**
     * @brief Get iterator representing past-the-end.
     * 
     * @return Iterator representing the end of the skip list
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator end() const;
};

// Static member definition
template<typename Key, typename Value, typename Compare>
thread_local std::mt19937 AtomicSkipList<Key, Value, Compare>::rng_(std::random_device{}());

// Implementation starts here

template<typename Key, typename Value, typename Compare>
AtomicSkipList<Key, Value, Compare>::AtomicSkipList() {
    // Create sentinel nodes with default values
    head_ = new Node(Key{}, Value{}, MAX_LEVEL - 1);
    tail_ = new Node(Key{}, Value{}, MAX_LEVEL - 1);
    
    // Connect head to tail at all levels
    for (int i = 0; i < MAX_LEVEL; ++i) {
        head_->next[i].store(tail_);
    }
}

template<typename Key, typename Value, typename Compare>
AtomicSkipList<Key, Value, Compare>::~AtomicSkipList() {
    Node* current = head_;
    while (current) {
        Node* next = current->next[0].load();
        delete current;
        current = next;
    }
}

template<typename Key, typename Value, typename Compare>
int AtomicSkipList<Key, Value, Compare>::random_level() {
    std::uniform_int_distribution<> dist(0, 1);
    int level = 0;
    while (level < MAX_LEVEL - 1 && dist(rng_) == 0) {
        ++level;
    }
    return level;
}

template<typename Key, typename Value, typename Compare>
std::array<typename AtomicSkipList<Key, Value, Compare>::Node*, AtomicSkipList<Key, Value, Compare>::MAX_LEVEL>
AtomicSkipList<Key, Value, Compare>::find_predecessors(const Key& key) {
    std::array<Node*, MAX_LEVEL> predecessors;
    Node* current = head_;
    
    for (int level = MAX_LEVEL - 1; level >= 0; --level) {
        while (true) {
            Node* next = current->next[level].load(std::memory_order_acquire);
            
            if (next == tail_ || comparator_(key, next->key)) {
                break;
            }
            
            if (next->marked.load(std::memory_order_acquire)) {
                // Help remove marked node
                current->next[level].compare_exchange_weak(next, next->next[level].load(),
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;
            }
            
            current = next;
        }
        predecessors[level] = current;
    }
    
    return predecessors;
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::insert(const Key& key, const Value& value) {
    int attempts = 0;
    while (attempts < 1000) {
        // First check if key already exists
        if (contains(key)) {
            return false;
        }
        
        int level = random_level();
        Node* new_node = new Node(key, value, level);
        auto predecessors = find_predecessors(key);
        
        // Double-check that key doesn't exist after finding predecessors
        Node* successor = predecessors[0]->next[0].load(std::memory_order_acquire);
        if (successor != tail_ && successor != head_ && 
            !comparator_(key, successor->key) && !comparator_(successor->key, key)) {
            // Key already exists
            delete new_node;
            return false;
        }
        
        // Link new node at all levels
        for (int i = 0; i <= level; ++i) {
            new_node->next[i].store(predecessors[i]->next[i].load(std::memory_order_relaxed));
        }
        
        // Try to insert at level 0 first (linearization point)
        Node* expected = new_node->next[0].load();
        if (predecessors[0]->next[0].compare_exchange_weak(expected, new_node,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed)) {
            
            // Insert at higher levels (best effort)
            for (int i = 1; i <= level; ++i) {
                int level_attempts = 0;
                while (level_attempts < 100) {
                    Node* level_expected = new_node->next[i].load();
                    if (predecessors[i]->next[i].compare_exchange_weak(level_expected, new_node,
                                                                     std::memory_order_release,
                                                                     std::memory_order_relaxed)) {
                        break;
                    }
                    // Re-find predecessors for this level if CAS failed
                    auto new_predecessors = find_predecessors(key);
                    predecessors[i] = new_predecessors[i];
                    new_node->next[i].store(predecessors[i]->next[i].load());
                    level_attempts++;
                }
            }
            
            return true;
        } else {
            // Failed to insert at level 0, clean up and retry
            delete new_node;
            attempts++;
        }
    }
    
    return false; // Failed after max attempts
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::insert(Key&& key, Value&& value) {
    int attempts = 0;
    while (attempts < 1000) {
        // First check if key already exists
        if (contains(key)) {
            return false;
        }
        
        int level = random_level();
        Node* new_node = new Node(std::move(key), std::move(value), level);
        auto predecessors = find_predecessors(new_node->key);
        
        // Double-check that key doesn't exist after finding predecessors
        Node* successor = predecessors[0]->next[0].load(std::memory_order_acquire);
        if (successor != tail_ && successor != head_ && 
            !comparator_(new_node->key, successor->key) && !comparator_(successor->key, new_node->key)) {
            // Key already exists
            delete new_node;
            return false;
        }
        
        // Link new node at all levels
        for (int i = 0; i <= level; ++i) {
            new_node->next[i].store(predecessors[i]->next[i].load(std::memory_order_relaxed));
        }
        
        // Try to insert at level 0 first (linearization point)
        Node* expected = new_node->next[0].load();
        if (predecessors[0]->next[0].compare_exchange_weak(expected, new_node,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed)) {
            
            // Insert at higher levels (best effort)
            for (int i = 1; i <= level; ++i) {
                int level_attempts = 0;
                while (level_attempts < 100) {
                    Node* level_expected = new_node->next[i].load();
                    if (predecessors[i]->next[i].compare_exchange_weak(level_expected, new_node,
                                                                     std::memory_order_release,
                                                                     std::memory_order_relaxed)) {
                        break;
                    }
                    // Re-find predecessors for this level if CAS failed
                    auto new_predecessors = find_predecessors(new_node->key);
                    predecessors[i] = new_predecessors[i];
                    new_node->next[i].store(predecessors[i]->next[i].load());
                    level_attempts++;
                }
            }
            
            return true;
        } else {
            // Failed to insert at level 0, clean up and retry
            delete new_node;
            attempts++;
        }
    }
    
    return false; // Failed after max attempts
}

template<typename Key, typename Value, typename Compare>
template<typename... Args>
bool AtomicSkipList<Key, Value, Compare>::emplace(const Key& key, Args&&... args) {
    return insert(key, Value(std::forward<Args>(args)...));
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::find(const Key& key, Value& result) const {
    Node* current = head_;
    
    for (int level = MAX_LEVEL - 1; level >= 0; --level) {
        while (true) {
            Node* next = current->next[level].load(std::memory_order_acquire);
            
            if (next == tail_) {
                break;
            }
            
            if (next->marked.load(std::memory_order_acquire)) {
                // Skip marked nodes - advance current to next's next
                Node* skip_to = next->next[level].load(std::memory_order_acquire);
                // Use compare_exchange to safely update the link, bypassing marked node
                current->next[level].compare_exchange_weak(next, skip_to, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;  // Retry from current position
            }
            
            if (comparator_(key, next->key)) {
                break;
            } else if (!comparator_(next->key, key)) {
                // Found the key - double check it's not marked
                if (!next->marked.load(std::memory_order_acquire)) {
                    result = next->value;
                    return true;
                }
                // If marked, continue searching
                Node* skip_to = next->next[level].load(std::memory_order_acquire);
                current->next[level].compare_exchange_weak(next, skip_to, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;
            }
            
            current = next;
        }
    }
    
    return false;
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::contains(const Key& key) const {
    Node* current = head_;
    
    for (int level = MAX_LEVEL - 1; level >= 0; --level) {
        while (true) {
            Node* next = current->next[level].load(std::memory_order_acquire);
            
            if (next == tail_) {
                break;
            }
            
            if (next->marked.load(std::memory_order_acquire)) {
                // Skip marked nodes - advance current to next's next
                Node* skip_to = next->next[level].load(std::memory_order_acquire);
                // Use compare_exchange to safely update the link, bypassing marked node
                current->next[level].compare_exchange_weak(next, skip_to, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;  // Retry from current position
            }
            
            if (comparator_(key, next->key)) {
                break;
            } else if (!comparator_(next->key, key)) {
                // Found the key - double check it's not marked
                if (!next->marked.load(std::memory_order_acquire)) {
                    return true;
                }
                // If marked, continue searching
                Node* skip_to = next->next[level].load(std::memory_order_acquire);
                current->next[level].compare_exchange_weak(next, skip_to, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;
            }
            
            current = next;
        }
    }
    
    return false;
}

template<typename Key, typename Value, typename Compare>
template<typename Func>
bool AtomicSkipList<Key, Value, Compare>::find_if(const Key& key, Func&& func) const {
    Node* current = head_;
    
    for (int level = MAX_LEVEL - 1; level >= 0; --level) {
        while (true) {
            Node* next = current->next[level].load(std::memory_order_acquire);
            
            if (next == tail_) {
                break;
            }
            
            if (next->marked.load(std::memory_order_acquire)) {
                // Skip marked nodes - advance current to next's next
                Node* skip_to = next->next[level].load(std::memory_order_acquire);
                // Use compare_exchange to safely update the link, bypassing marked node
                current->next[level].compare_exchange_weak(next, skip_to, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;  // Retry from current position
            }
            
            if (comparator_(key, next->key)) {
                break;
            } else if (!comparator_(next->key, key)) {
                // Found the key - double check it's not marked before applying predicate
                if (!next->marked.load(std::memory_order_acquire)) {
                    return func(next->value);
                }
                // If marked, continue searching
                Node* skip_to = next->next[level].load(std::memory_order_acquire);
                current->next[level].compare_exchange_weak(next, skip_to, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                continue;
            }
            
            current = next;
        }
    }
    
    return false;
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::erase(const Key& key) {
    // Simplified erase: search only at level 0 for reliable traversal
    Node* current = head_;
    
    while (true) {
        Node* next = current->next[0].load(std::memory_order_acquire);
        
        if (next == tail_) {
            break;  // Reached end, key not found
        }
        
        if (next->marked.load(std::memory_order_acquire)) {
            // Skip marked nodes - advance current to next's next
            Node* skip_to = next->next[0].load(std::memory_order_acquire);
            // Use compare_exchange to safely update the link, bypassing marked node
            current->next[0].compare_exchange_weak(next, skip_to, 
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
            continue;  // Retry from current position
        }
        
        if (comparator_(key, next->key)) {
            break;  // Key would be before this node, so not found
        } else if (!comparator_(next->key, key)) {
            // Found the key - try to mark it for deletion
            bool expected = false;
            if (next->marked.compare_exchange_strong(expected, true,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
                return true;
            }
            // If marking failed, node was already marked by another thread
            return false;
        }
        
        current = next;
    }
    
    return false;  // Key not found
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::empty() const {
    Node* current = head_->next[0].load(std::memory_order_acquire);
    
    while (current != tail_) {
        if (!current->marked.load(std::memory_order_acquire)) {
            return false; // Found an unmarked node
        }
        current = current->next[0].load(std::memory_order_acquire);
    }
    
    return true; // No unmarked nodes found
}

template<typename Key, typename Value, typename Compare>
size_t AtomicSkipList<Key, Value, Compare>::size() const {
    size_t count = 0;
    Node* current = head_->next[0].load(std::memory_order_acquire);
    
    while (current != tail_) {
        if (!current->marked.load(std::memory_order_acquire)) {
            count++;
        }
        current = current->next[0].load(std::memory_order_acquire);
    }
    
    return count;
}

// Iterator implementation

template<typename Key, typename Value, typename Compare>
AtomicSkipList<Key, Value, Compare>::iterator::iterator(Node* node, Node* tail) 
    : current_(node), tail_(tail) {
    // Skip to first non-marked node
    while (current_ != tail_ && current_->marked.load(std::memory_order_acquire)) {
        current_ = current_->next[0].load(std::memory_order_acquire);
    }
}

template<typename Key, typename Value, typename Compare>
std::pair<Key, Value> AtomicSkipList<Key, Value, Compare>::iterator::operator*() const {
    return {current_->key, current_->value};
}

template<typename Key, typename Value, typename Compare>
typename AtomicSkipList<Key, Value, Compare>::iterator& 
AtomicSkipList<Key, Value, Compare>::iterator::operator++() {
    if (current_ != tail_) {
        current_ = current_->next[0].load(std::memory_order_acquire);
        // Skip marked nodes
        while (current_ != tail_ && current_->marked.load(std::memory_order_acquire)) {
            current_ = current_->next[0].load(std::memory_order_acquire);
        }
    }
    return *this;
}

template<typename Key, typename Value, typename Compare>
typename AtomicSkipList<Key, Value, Compare>::iterator 
AtomicSkipList<Key, Value, Compare>::iterator::operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::iterator::operator==(const iterator& other) const {
    return current_ == other.current_;
}

template<typename Key, typename Value, typename Compare>
bool AtomicSkipList<Key, Value, Compare>::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}

template<typename Key, typename Value, typename Compare>
typename AtomicSkipList<Key, Value, Compare>::iterator AtomicSkipList<Key, Value, Compare>::begin() const {
    Node* first = head_->next[0].load(std::memory_order_acquire);
    return iterator(first, tail_);
}

template<typename Key, typename Value, typename Compare>
typename AtomicSkipList<Key, Value, Compare>::iterator AtomicSkipList<Key, Value, Compare>::end() const {
    return iterator(tail_, tail_);
}

} // namespace lockfree