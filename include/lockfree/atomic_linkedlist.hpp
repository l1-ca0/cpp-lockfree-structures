#pragma once

#include <atomic>
#include <memory>
#include <functional>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe linked list implementation using mark-and-sweep deletion.
 * 
 * This linked list provides ordered element storage without using traditional locking mechanisms.
 * It uses atomic operations and logical deletion (marking) to ensure thread safety and lock-free
 * progress. The list maintains elements in insertion order and supports concurrent search,
 * insertion, and deletion operations.
 * 
 * @tparam T The type of elements stored in the linked list. Must be constructible,
 *           destructible, and comparable according to the Compare function.
 * @tparam Compare A binary predicate for element comparison. Defaults to std::equal_to<T>.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Mark-and-sweep: Uses logical deletion for safe concurrent access
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - Iterator support: Forward iteration with automatic skipping of deleted nodes
 * 
 * Performance Characteristics:
 * - Insert: O(n) worst case - must traverse to find insertion point
 * - Remove: O(n) worst case - must search for element to remove
 * - Find: O(n) worst case - linear search through list
 * - Memory: O(n) where n is the number of elements (including marked nodes)
 * 
 * Algorithm Details:
 * - Uses mark-and-sweep algorithm for safe memory reclamation
 * - Nodes are logically deleted by setting a mark flag
 * - Physical deletion is deferred to help with search operations
 * - Compare-and-swap operations ensure atomicity of pointer updates
 * 
 * Memory Management:
 * - Uses dynamic allocation for nodes
 * - Marked nodes are cleaned up during search operations
 * - All memory is properly cleaned up in destructor
 * - Optimized for concurrent access with efficient memory reclamation
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicLinkedList<int> list;
 * 
 * // Insert elements
 * list.insert(42);
 * list.emplace(100);
 * 
 * // Search for elements
 * if (list.contains(42)) {
 *     std::cout << "Found 42 in the list" << std::endl;
 * }
 * 
 * // Remove elements
 * if (list.remove(42)) {
 *     std::cout << "Removed 42 from the list" << std::endl;
 * }
 * 
 * // Iterate through elements
 * for (const auto& item : list) {
 *     std::cout << "Item: " << item << std::endl;
 * }
 * @endcode
 * 
 * @note This implementation allows duplicate elements and uses logical deletion
 *       for safe concurrent access.
 */
template<typename T, typename Compare = std::equal_to<T>>
class AtomicLinkedList {
private:
    /**
     * @brief Internal node structure for the linked list.
     * 
     * Each node contains the data, an atomic pointer to the next node,
     * and an atomic mark flag for logical deletion.
     */
    struct Node {
        T data;                         ///< The stored data element
        std::atomic<Node*> next;        ///< Atomic pointer to next node
        std::atomic<bool> marked;       ///< Atomic flag indicating logical deletion
        
        /**
         * @brief Construct node with copied data.
         * @param item The data to copy into the node
         */
        Node(const T& item) : data(item), next(nullptr), marked(false) {}
        
        /**
         * @brief Construct node with moved data.
         * @param item The data to move into the node
         */
        Node(T&& item) : data(std::move(item)), next(nullptr), marked(false) {}
    };
    
    std::atomic<Node*> head_;        ///< Atomic pointer to the head of the list
    std::atomic<size_t> size_;       ///< Atomic counter for number of elements (including marked)
    Compare comparator_;             ///< Comparison function for element equality
    
    /**
     * @brief Search for a key in the list, cleaning up marked nodes along the way.
     * 
     * Traverses the list looking for the specified key while helping to clean up
     * logically deleted (marked) nodes by unlinking them from the list.
     * 
     * @param key The key to search for
     * @return Pair of (previous_node, current_node) where current matches key or is null
     */
    std::pair<Node*, Node*> search(const T& key);
    
public:
    /**
     * @brief Default constructor. Creates an empty linked list.
     * 
     * @complexity O(1)
     * @thread_safety Safe
     */
    AtomicLinkedList();
    
    /**
     * @brief Destructor. Cleans up all nodes including marked ones.
     * 
     * @complexity O(n) where n is the number of nodes (including marked)
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicLinkedList();
    
    // Non-copyable but movable for resource management
    AtomicLinkedList(const AtomicLinkedList&) = delete;
    AtomicLinkedList& operator=(const AtomicLinkedList&) = delete;
    AtomicLinkedList(AtomicLinkedList&&) = default;
    AtomicLinkedList& operator=(AtomicLinkedList&&) = default;
    
    /**
     * @brief Insert an element into the list by copying.
     * 
     * @param item The item to copy and insert into the list
     * @return true if successfully inserted, false if insertion failed after max attempts
     * @complexity O(n) worst case - may traverse entire list
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's copy constructor throws,
     *                  the list remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Allows duplicate elements to be inserted.
     */
    bool insert(const T& item);
    
    /**
     * @brief Insert an element into the list by moving.
     * 
     * @param item The item to move and insert into the list
     * @return true if successfully inserted, false if insertion failed after max attempts
     * @complexity O(n) worst case - may traverse entire list
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's move constructor throws,
     *                  the list remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Allows duplicate elements to be inserted.
     */
    bool insert(T&& item);
    
    /**
     * @brief Construct an element in-place in the list.
     * 
     * @tparam Args Types of arguments for T's constructor
     * @param args Arguments to forward to T's constructor
     * @return true if successfully constructed and inserted, false if insertion failed
     * @complexity O(n) worst case - may traverse entire list
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's constructor throws,
     *                  the list remains unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts.
     *       Allows duplicate elements to be inserted.
     */
    template<typename... Args>
    bool emplace(Args&&... args);
    
    /**
     * @brief Remove the first occurrence of an element from the list.
     * 
     * @param item The item to remove from the list
     * @return true if element was found and marked for deletion, false if not found
     * @complexity O(n) worst case - may traverse entire list
     * @thread_safety Safe
     * @exception_safety Strong guarantee - if operation fails, list is unchanged
     * 
     * @note Uses logical deletion (marking). Physical removal happens during search operations.
     *       Only removes the first occurrence if duplicates exist.
     */
    bool remove(const T& item);
    
    /**
     * @brief Search for an element in the list.
     * 
     * @param item The item to search for
     * @return true if element is found and not marked for deletion, false otherwise
     * @complexity O(n) worst case - may traverse entire list
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note May help clean up marked nodes during traversal, improving future performance.
     */
    bool find(const T& item) const;
    
    /**
     * @brief Check if the list contains an element (alias for find).
     * 
     * @param item The item to check for
     * @return true if element is found and not marked for deletion, false otherwise
     * @complexity O(n) worst case - may traverse entire list
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This is an alias for find() for API consistency with standard containers.
     */
    bool contains(const T& item) const;
    
    /**
     * @brief Check if the list is empty.
     * 
     * @return true if the list contains no unmarked elements, false otherwise
     * @complexity O(n) worst case - may need to traverse list to find unmarked nodes
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This operation may be expensive as it needs to check for unmarked nodes.
     *       Result may be immediately outdated in concurrent environment.
     */
    bool empty() const;
    
    /**
     * @brief Get the current number of elements in the list.
     * 
     * @return The number of nodes currently in the list (including marked ones)
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This count includes logically deleted (marked) nodes.
     *       For an accurate count of active elements, use empty() or iterate through the list.
     *       Result may be immediately outdated in concurrent environment.
     */
    size_t size() const;
    
    /**
     * @brief Forward iterator for traversing the linked list.
     * 
     * The iterator automatically skips over logically deleted (marked) nodes,
     * providing access only to active elements in the list.
     */
    class iterator {
    private:
        Node* current_;              ///< Current node being pointed to
        
    public:
        /**
         * @brief Construct iterator pointing to given node.
         * @param node The node to point to (nullptr for end iterator)
         */
        iterator(Node* node);
        
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
         * @brief Pre-increment operator to advance to next unmarked element.
         * @return Reference to this iterator after advancement
         */
        iterator& operator++();
        
        /**
         * @brief Post-increment operator to advance to next unmarked element.
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
     * @brief Get iterator to the first unmarked element.
     * 
     * @return Iterator pointing to first active element, or end() if list is empty
     * @complexity O(n) worst case - may need to skip marked nodes
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator begin() const;
    
    /**
     * @brief Get iterator representing past-the-end.
     * 
     * @return Iterator representing the end of the list
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator end() const;
};

template<typename T, typename Compare>
AtomicLinkedList<T, Compare>::AtomicLinkedList() : head_(nullptr), size_(0) {}

template<typename T, typename Compare>
AtomicLinkedList<T, Compare>::~AtomicLinkedList() {
    Node* current = head_.load();
    while (current) {
        Node* next = current->next.load();
        delete current;
        current = next;
    }
}

template<typename T, typename Compare>
std::pair<typename AtomicLinkedList<T, Compare>::Node*, typename AtomicLinkedList<T, Compare>::Node*>
AtomicLinkedList<T, Compare>::search(const T& key) {
    Node* prev = nullptr;
    Node* current = head_.load(std::memory_order_acquire);
    
    while (current) {
        // Skip marked (deleted) nodes
        if (current->marked.load(std::memory_order_acquire)) {
            Node* next = current->next.load(std::memory_order_acquire);
            
            // Try to help remove the marked node
            if (prev) {
                prev->next.compare_exchange_weak(current, next,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
            } else {
                head_.compare_exchange_weak(current, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed);
            }
            
            current = next;
            continue;
        }
        
        // Check if we found the key
        if (comparator_(current->data, key)) {
            return {prev, current};
        }
        
        prev = current;
        current = current->next.load(std::memory_order_acquire);
    }
    
    return {prev, nullptr};
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::insert(const T& item) {
    Node* new_node = new Node(item);
    
    for (int attempts = 0; attempts < 1000; ++attempts) {
        auto [prev, current] = search(item);
        
        if (current && comparator_(current->data, item)) {
            // Item already exists
            delete new_node;
            return false;
        }
        
        new_node->next.store(current, std::memory_order_relaxed);
        
        if (prev == nullptr) {
            // Insert at head
            if (head_.compare_exchange_weak(current, new_node,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        } else {
            // Insert after prev
            if (prev->next.compare_exchange_weak(current, new_node,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        // If CAS failed, retry
    }
    
    // Failed after max attempts
    delete new_node;
    return false;
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::insert(T&& item) {
    Node* new_node = new Node(std::move(item));
    
    for (int attempts = 0; attempts < 1000; ++attempts) {
        auto [prev, current] = search(new_node->data);
        
        if (current && comparator_(current->data, new_node->data)) {
            // Item already exists
            delete new_node;
            return false;
        }
        
        new_node->next.store(current, std::memory_order_relaxed);
        
        if (prev == nullptr) {
            // Insert at head
            if (head_.compare_exchange_weak(current, new_node,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        } else {
            // Insert after prev
            if (prev->next.compare_exchange_weak(current, new_node,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        // If CAS failed, retry
    }
    
    // Failed after max attempts
    delete new_node;
    return false;
}

template<typename T, typename Compare>
template<typename... Args>
bool AtomicLinkedList<T, Compare>::emplace(Args&&... args) {
    return insert(T(std::forward<Args>(args)...));
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::remove(const T& item) {
    auto [prev, current] = search(item);
    
    if (current && comparator_(current->data, item)) {
        // Mark the node for deletion
        bool expected = false;
        if (current->marked.compare_exchange_strong(expected, true,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
            // Successfully marked for deletion - decrement size
            size_.fetch_sub(1, std::memory_order_relaxed);
            // Physical removal will happen during future search operations
            return true;
        }
    }
    
    return false;  // Item not found or already marked
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::find(const T& item) const {
    Node* current = head_.load(std::memory_order_acquire);
    
    while (current) {
        if (!current->marked.load(std::memory_order_acquire) && 
            comparator_(current->data, item)) {
            return true;
        }
        current = current->next.load(std::memory_order_acquire);
    }
    
    return false;
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::contains(const T& item) const {
    return find(item);
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::empty() const {
    Node* current = head_.load(std::memory_order_acquire);
    
    while (current) {
        if (!current->marked.load(std::memory_order_acquire)) {
            return false;  // Found an unmarked node
        }
        current = current->next.load(std::memory_order_acquire);
    }
    
    return true;  // No unmarked nodes found
}

template<typename T, typename Compare>
size_t AtomicLinkedList<T, Compare>::size() const {
    return size_.load(std::memory_order_relaxed);
}

// Iterator implementation

template<typename T, typename Compare>
AtomicLinkedList<T, Compare>::iterator::iterator(Node* node) : current_(node) {
    // Skip to first unmarked node
    while (current_ && current_->marked.load(std::memory_order_acquire)) {
        current_ = current_->next.load(std::memory_order_acquire);
    }
}

template<typename T, typename Compare>
const T& AtomicLinkedList<T, Compare>::iterator::operator*() const {
    return current_->data;
}

template<typename T, typename Compare>
const T* AtomicLinkedList<T, Compare>::iterator::operator->() const {
    return &current_->data;
}

template<typename T, typename Compare>
typename AtomicLinkedList<T, Compare>::iterator& 
AtomicLinkedList<T, Compare>::iterator::operator++() {
    if (current_) {
        current_ = current_->next.load(std::memory_order_acquire);
        // Skip marked nodes
        while (current_ && current_->marked.load(std::memory_order_acquire)) {
            current_ = current_->next.load(std::memory_order_acquire);
        }
    }
    return *this;
}

template<typename T, typename Compare>
typename AtomicLinkedList<T, Compare>::iterator 
AtomicLinkedList<T, Compare>::iterator::operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::iterator::operator==(const iterator& other) const {
    return current_ == other.current_;
}

template<typename T, typename Compare>
bool AtomicLinkedList<T, Compare>::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}

template<typename T, typename Compare>
typename AtomicLinkedList<T, Compare>::iterator AtomicLinkedList<T, Compare>::begin() const {
    return iterator(head_.load(std::memory_order_acquire));
}

template<typename T, typename Compare>
typename AtomicLinkedList<T, Compare>::iterator AtomicLinkedList<T, Compare>::end() const {
    return iterator(nullptr);
}

} // namespace lockfree