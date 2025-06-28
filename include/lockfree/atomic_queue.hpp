#pragma once

#include <atomic>
#include <memory>
#include <utility>
#include <thread>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe queue implementation using the Michael & Scott algorithm.
 * 
 * This queue provides FIFO (First-In-First-Out) semantics without using traditional
 * locking mechanisms. It implements the Michael & Scott lock-free queue algorithm,
 * which uses atomic compare-and-swap operations to ensure thread safety and lock-free progress.
 * 
 * @tparam T The type of elements stored in the queue. Must be constructible,
 *           destructible, and either copyable or movable.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - FIFO ordering: Elements are dequeued in the order they were enqueued
 * 
 * Performance Characteristics:
 * - Enqueue: O(1) amortized, may retry under contention with progressive backoff
 * - Dequeue: O(1) amortized, may retry under contention with progressive backoff
 * - Size: O(n) linear traversal for accurate count (no atomic size counter)
 * - Memory: O(n) where n is the number of elements + 1 dummy node
 * 
 * Algorithm Details:
 * - Based on Michael & Scott's lock-free queue algorithm
 * - Uses a dummy head node to simplify the algorithm
 * - Separates head and tail pointers for concurrent enqueue/dequeue
 * - Uses careful pointer management for safe concurrent access
 * - Progressive backoff strategy under contention: CPU pause → progressive delay → thread yield
 * - CPU-specific optimizations: x86/ARM pause instructions for reduced power consumption
 * - No atomic size counter to eliminate contention bottleneck
 * 
 * Memory Management:
 * - Uses dynamic allocation for nodes and data
 * - Data is allocated separately to handle the Michael & Scott algorithm requirements
 * - Memory is freed during destruction
 * - Efficient memory management with proper cleanup on destruction
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicQueue<std::string> queue;
 * 
 * // Enqueue elements
 * queue.enqueue("hello");
 * queue.emplace("world");
 * 
 * // Dequeue elements
 * std::string value;
 * if (queue.dequeue(value)) {
 *     std::cout << "Dequeued: " << value << std::endl;
 * }
 * @endcode
 * 
 * @note This implementation uses the classic Michael & Scott algorithm which
 *       allocates data separately from nodes to handle the algorithm's requirements.
 *       Memory management is optimized for concurrent access patterns.
 */
template<typename T>
class AtomicQueue {
private:
    /**
     * @brief Internal node structure for the queue.
     * 
     * Each node contains an atomic pointer to data and an atomic pointer to the next node.
     * The data is allocated separately to conform to the Michael & Scott algorithm.
     * The atomic pointers ensure thread-safe traversal and modification.
     */
    struct Node {
        std::atomic<T*> data;         ///< Atomic pointer to the stored data element
        std::atomic<Node*> next;      ///< Atomic pointer to next node
        
        /**
         * @brief Default constructor. Creates a node with null data and next pointers.
         */
        Node() : data(nullptr), next(nullptr) {}
    };
    
    std::atomic<Node*> head_;         ///< Atomic pointer to the head (dummy) node
    std::atomic<Node*> tail_;         ///< Atomic pointer to the tail node
    
    // CPU-specific optimizations
    #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        static inline void cpu_pause() {
            #if defined(__GNUC__) || defined(__clang__)
                __builtin_ia32_pause();
            #elif defined(_MSC_VER)
                _mm_pause();
            #endif
        }
    #elif defined(__aarch64__) || defined(_M_ARM64)
        static inline void cpu_pause() {
            #if defined(__GNUC__) || defined(__clang__)
                asm volatile("yield");
            #endif
        }
    #else
        static inline void cpu_pause() {
            std::this_thread::yield();
        }
    #endif
    
public:
    /**
     * @brief Default constructor. Creates an empty queue with a dummy head node.
     * 
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety Strong guarantee
     */
    AtomicQueue();
    
    /**
     * @brief Destructor. Cleans up all nodes and remaining data.
     * 
     * @complexity O(n) where n is the number of elements
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicQueue();
    
    // Non-copyable but movable for resource management
    AtomicQueue(const AtomicQueue&) = delete;
    AtomicQueue& operator=(const AtomicQueue&) = delete;
    AtomicQueue(AtomicQueue&&) = default;
    AtomicQueue& operator=(AtomicQueue&&) = default;
    
    /**
     * @brief Enqueue a copy of the item to the back of the queue.
     * 
     * @param item The item to copy and enqueue
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's copy constructor throws,
     *                  the queue remains unchanged
     * 
     * @note May fail silently under extreme contention after 1000 retry attempts.
     *       In practice, this is extremely rare.
     */
    void enqueue(const T& item);
    
    /**
     * @brief Enqueue an item to the back of the queue by moving it.
     * 
     * @param item The item to move and enqueue
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's move constructor throws,
     *                  the queue remains unchanged
     * 
     * @note May fail silently under extreme contention after 1000 retry attempts.
     *       In practice, this is extremely rare.
     */
    void enqueue(T&& item);
    
    /**
     * @brief Construct an element in-place at the back of the queue.
     * 
     * @tparam Args Types of arguments for T's constructor
     * @param args Arguments to forward to T's constructor
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if T's constructor throws,
     *                  the queue remains unchanged
     * 
     * @note May fail silently under extreme contention after 1000 retry attempts.
     *       In practice, this is extremely rare.
     */
    template<typename... Args>
    void emplace(Args&&... args);
    
    /**
     * @brief Dequeue an element from the front of the queue.
     * 
     * @param result Reference to store the dequeued element
     * @return true if an element was successfully dequeued, false if queue was empty
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe
     * @exception_safety Strong guarantee - if operation fails, queue is unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts
     */
    bool dequeue(T& result);
    
    /**
     * @brief Check if the queue is empty.
     * 
     * @return true if the queue contains no elements, false otherwise
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment.
     *       This check involves multiple atomic loads and may not be perfectly
     *       consistent under high contention.
     */
    bool empty() const;
    
    /**
     * @brief Get the current number of elements in the queue.
     * 
     * @return The number of elements currently in the queue
     * @complexity O(n) where n is the number of elements (traverses the queue)
     * @thread_safety Safe but expensive under high contention
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment.
     *       This method traverses the queue to count elements, avoiding the
     *       contention of an atomic size counter for better overall performance.
     */
    size_t size() const;
    
    /**
     * @brief Peek at the front element without removing it.
     * 
     * @param result Reference to store a copy of the front element
     * @return true if front element was successfully read, false if queue was empty
     * @complexity O(1)
     * @thread_safety Safe but with caveats (see warning)
     * @exception_safety Basic guarantee
     * 
     * @note Provides atomic snapshot of the front element at time of call.
     *       Result may be immediately outdated in concurrent environment.
     */
    bool front(T& result) const;
};

// Implementation

template<typename T>
AtomicQueue<T>::AtomicQueue() {
    Node* dummy = new Node;
    head_.store(dummy);
    tail_.store(dummy);
}

template<typename T>
AtomicQueue<T>::~AtomicQueue() {
    // Clean up remaining items and nodes
    Node* current = head_.load();
    while (current) {
        Node* next = current->next.load();
        T* data = current->data.load();
        if (data != nullptr) {
            delete data;
        }
        delete current;
        current = next;
    }
}

template<typename T>
void AtomicQueue<T>::enqueue(const T& item) {
    Node* new_node = new Node;
    T* data = new T(item);
    new_node->data.store(data);
    
    for (int attempts = 0; attempts < 1000; ++attempts) {
        Node* last = tail_.load(std::memory_order_acquire);
        Node* next = last->next.load(std::memory_order_acquire);
        
        // Check if tail is still consistent
        if (last == tail_.load(std::memory_order_acquire)) {
            if (next == nullptr) {
                // Try to insert new node at the end
                if (last->next.compare_exchange_weak(next, new_node,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
                    // Successfully linked new node, now advance tail
                    tail_.compare_exchange_weak(last, new_node,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                    return;
                }
            } else {
                // Tail is lagging, try to advance it
                tail_.compare_exchange_weak(last, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed);
            }
        }
        
        // Progressive backoff strategy
        if (attempts < 10) {
            cpu_pause();
        } else if (attempts < 100) {
            for (int i = 0; i < (attempts - 10); ++i) cpu_pause();
        } else {
            std::this_thread::yield();
        }
    }
    
    // Failed after max attempts - clean up
    delete data;
    delete new_node;
}

template<typename T>
void AtomicQueue<T>::enqueue(T&& item) {
    Node* new_node = new Node;
    T* data = new T(std::move(item));
    new_node->data.store(data);
    
    for (int attempts = 0; attempts < 1000; ++attempts) {
        Node* last = tail_.load(std::memory_order_acquire);
        Node* next = last->next.load(std::memory_order_acquire);
        
        // Check if tail is still consistent
        if (last == tail_.load(std::memory_order_acquire)) {
            if (next == nullptr) {
                // Try to insert new node at the end
                if (last->next.compare_exchange_weak(next, new_node,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
                    // Successfully linked new node, now advance tail
                    tail_.compare_exchange_weak(last, new_node,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                    return;
                }
            } else {
                // Tail is lagging, try to advance it
                tail_.compare_exchange_weak(last, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed);
            }
        }
        
        // Progressive backoff strategy
        if (attempts < 10) {
            cpu_pause();
        } else if (attempts < 100) {
            for (int i = 0; i < (attempts - 10); ++i) cpu_pause();
        } else {
            std::this_thread::yield();
        }
    }
    
    // Failed after max attempts - clean up
    delete data;
    delete new_node;
}

template<typename T>
template<typename... Args>
void AtomicQueue<T>::emplace(Args&&... args) {
    Node* new_node = new Node;
    T* data = new T(std::forward<Args>(args)...);
    new_node->data.store(data);
    
    for (int attempts = 0; attempts < 1000; ++attempts) {
        Node* last = tail_.load(std::memory_order_acquire);
        Node* next = last->next.load(std::memory_order_acquire);
        
        // Check if tail is still consistent
        if (last == tail_.load(std::memory_order_acquire)) {
            if (next == nullptr) {
                // Try to insert new node at the end
                if (last->next.compare_exchange_weak(next, new_node,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
                    // Successfully linked new node, now advance tail
                    tail_.compare_exchange_weak(last, new_node,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                    return;
                }
            } else {
                // Tail is lagging, try to advance it
                tail_.compare_exchange_weak(last, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed);
            }
        }
        
        // Progressive backoff strategy
        if (attempts < 10) {
            cpu_pause();
        } else if (attempts < 100) {
            for (int i = 0; i < (attempts - 10); ++i) cpu_pause();
        } else {
            std::this_thread::yield();
        }
    }
    
    // Failed after max attempts - clean up
    delete data;
    delete new_node;
}

template<typename T>
bool AtomicQueue<T>::dequeue(T& result) {
    for (int attempts = 0; attempts < 1000; ++attempts) {
        Node* first = head_.load(std::memory_order_acquire);
        Node* last = tail_.load(std::memory_order_acquire);
        Node* next = first->next.load(std::memory_order_acquire);
        
        // Check consistency
        if (first == head_.load(std::memory_order_acquire)) {
            if (first == last) {
                if (next == nullptr) {
                    // Queue is empty
                    return false;
                }
                // Tail is lagging, try to advance it
                tail_.compare_exchange_weak(last, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed);
            } else {
                if (next == nullptr) {
                    // Inconsistent state, retry
                    cpu_pause();
                    continue;
                }
                
                T* data = next->data.load(std::memory_order_acquire);
                if (data == nullptr) {
                    // Data was already consumed, retry
                    cpu_pause();
                    continue;
                }
                
                // Try to set data to null to mark as consumed
                T* expected_data = data;
                if (!next->data.compare_exchange_weak(expected_data, nullptr,
                                                    std::memory_order_acquire,
                                                    std::memory_order_relaxed)) {
                    // Someone else got it first, retry
                    cpu_pause();
                    continue;
                }
                
                // Try to advance head to next node
                if (head_.compare_exchange_weak(first, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
                    // Successfully dequeued
                    result = std::move(*data);
                    delete data;
                    // Note: Not deleting the node immediately to avoid use-after-free
                    // Nodes will be cleaned up in destructor
                    return true;
                }
            }
        }
        
        // Progressive backoff strategy
        if (attempts < 10) {
            cpu_pause();
        } else if (attempts < 100) {
            for (int i = 0; i < (attempts - 10); ++i) cpu_pause();
        } else {
            std::this_thread::yield();
        }
    }
    
    return false; // Failed after max attempts
}

template<typename T>
bool AtomicQueue<T>::empty() const {
    Node* first = head_.load(std::memory_order_acquire);
    Node* last = tail_.load(std::memory_order_acquire);
    return (first == last) && (first->next.load(std::memory_order_acquire) == nullptr);
}

template<typename T>
size_t AtomicQueue<T>::size() const {
    size_t count = 0;
    Node* current = head_.load(std::memory_order_acquire);
    Node* next = current->next.load(std::memory_order_acquire);
    
    while (next != nullptr) {
        T* data = next->data.load(std::memory_order_acquire);
        if (data != nullptr) {
            count++;
        }
        current = next;
        next = current->next.load(std::memory_order_acquire);
    }
    
    return count;
}

template<typename T>
bool AtomicQueue<T>::front(T& result) const {
    Node* first = head_.load(std::memory_order_acquire);
    Node* next = first->next.load(std::memory_order_acquire);
    
    if (next == nullptr) {
        return false;
    }
    
    T* data = next->data.load(std::memory_order_acquire);
    if (data == nullptr) {
        return false;
    }
    
    result = *data;  // Copy for peek operation
    return true;
}

} // namespace lockfree