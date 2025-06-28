#pragma once

#include <atomic>
#include <memory>
#include <array>

namespace lockfree {

/**
 * @brief A lock-free work-stealing deque implementation based on the Chase-Lev algorithm.
 * 
 * This deque is specifically designed for work-stealing scenarios where a single
 * owner thread produces tasks and multiple worker (thief) threads steal tasks for
 * load balancing. It provides asymmetric access patterns optimized for this use case.
 * 
 * @tparam T The type of elements stored in the deque. Elements are stored as pointers
 *           and must be constructible, destructible, and either copyable or movable.
 *           The deque manages memory allocation internally.
 * 
 * Key Features:
 * - Lock-free: No blocking operations for the core work-stealing operations
 * - Asymmetric design: Optimized for single producer, multiple consumer patterns
 * - LIFO owner access: Owner thread pops from the same end it pushes (work locality)
 * - FIFO thief access: Thieves steal oldest tasks first (load balancing)
 * - Fixed capacity: 4095 elements to avoid complex resize operations
 * - Memory aligned: Cache-aligned data structures for optimal performance
 * 
 * Performance Characteristics:
 * - Push Bottom (owner): O(1) - constant time insertion
 * - Pop Bottom (owner): O(1) - constant time removal
 * - Steal (thieves): O(1) - constant time removal from opposite end
 * - Empty check: O(1) - constant time status check
 * - Size: O(1) - approximate count with atomic loads
 * - Memory: O(4096) - fixed capacity circular buffer
 * 
 * Algorithm Details:
 * - Based on Chase-Lev work-stealing deque algorithm
 * - Uses two atomic counters: top (for thieves) and bottom (for owner)
 * - Handles single-element race condition between owner and thieves
 * - Uses memory fences for proper visibility of operations
 * - Fixed-size circular buffer to avoid ABA problems with resizing
 * 
 * Memory Management:
 * - Elements are allocated dynamically and stored as pointers
 * - Automatic cleanup on destruction of remaining elements
 * - No memory reclamation during operation to maintain lock-free properties
 * - Cache-aligned slots to reduce false sharing between threads
 * 
 * Thread Safety:
 * - Owner thread: Only one thread should call push_bottom() and pop_bottom()
 * - Thief threads: Multiple threads can safely call steal() concurrently
 * - No synchronization needed between owner and thieves beyond the algorithm
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicWorkStealingDeque<Task> work_deque;
 * 
 * // Owner thread - pushes and processes own tasks
 * void owner_thread() {
 *     for (int i = 0; i < 1000; ++i) {
 *         work_deque.push_bottom(Task{i});
 *         
 *         // Occasionally process own work (LIFO - better cache locality)
 *         if (i % 10 == 0) {
 *             Task* task = work_deque.pop_bottom();
 *             if (task) {
 *                 process_task(*task);
 *                 delete task;
 *             }
 *         }
 *     }
 * }
 * 
 * // Worker threads - steal tasks from other end
 * void worker_thread() {
 *     while (running) {
 *         Task* task = work_deque.steal(); // FIFO - load balancing
 *         if (task) {
 *             process_task(*task);
 *             delete task;
 *         } else {
 *             std::this_thread::yield();
 *         }
 *     }
 * }
 * @endcode
 * 
 * @note This implementation uses a fixed capacity of 4095 elements to avoid
 *       the complexity of lock-free resizing. For applications requiring
 *       unlimited capacity, consider using multiple deques or a different
 *       data structure.
 * 
 * @warning The caller is responsible for managing the lifetime of elements.
 *          The deque returns raw pointers that must be deleted by the caller.
 *          Only call destructor when no other threads are accessing the deque.
 */
template<typename T>
class AtomicWorkStealingDeque {
private:
    static constexpr size_t CAPACITY = 4096;                    ///< Fixed capacity (power of 2)
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be a power of 2");
    
    /**
     * @brief Cache-aligned slot structure to store element pointers.
     * 
     * Each slot is cache-aligned (64 bytes) to prevent false sharing between
     * threads accessing different slots. The atomic pointer ensures thread-safe
     * access to the stored element.
     */
    struct alignas(64) Slot {
        std::atomic<T*> data{nullptr};           ///< Atomic pointer to stored element
        
        /**
         * @brief Default constructor initializes data to nullptr.
         */
        Slot() = default;
        
        // Non-copyable and non-movable to prevent accidental copying
        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;
        Slot(Slot&&) = delete;
        Slot& operator=(Slot&&) = delete;
    };
    
    alignas(64) std::atomic<size_t> top_{0};     ///< Top index for thief access (cache-aligned)
    alignas(64) std::atomic<size_t> bottom_{0};  ///< Bottom index for owner access (cache-aligned)
    
    std::array<Slot, CAPACITY> buffer_;          ///< Fixed-size circular buffer of slots
    
    static constexpr size_t INDEX_MASK = CAPACITY - 1;  ///< Mask for circular indexing
    
public:
    /**
     * @brief Default constructor. Creates an empty work-stealing deque.
     * 
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    AtomicWorkStealingDeque() = default;
    
    /**
     * @brief Destructor. Cleans up all remaining elements.
     * 
     * Automatically deletes any remaining elements in the deque to prevent
     * memory leaks. This should only be called when no other threads are
     * accessing the deque.
     * 
     * @complexity O(n) where n is the number of remaining elements
     * @thread_safety Not safe - should only be called when no other threads are accessing
     * @exception_safety No-throw guarantee
     */
    ~AtomicWorkStealingDeque() {
        // Clean up remaining elements
        while (!empty()) {
            T* element = pop_bottom();
            delete element;
        }
    }
    
    // Non-copyable and non-movable to prevent accidental sharing
    AtomicWorkStealingDeque(const AtomicWorkStealingDeque&) = delete;
    AtomicWorkStealingDeque& operator=(const AtomicWorkStealingDeque&) = delete;
    AtomicWorkStealingDeque(AtomicWorkStealingDeque&&) = delete;
    AtomicWorkStealingDeque& operator=(AtomicWorkStealingDeque&&) = delete;
    
    /**
     * @brief Push an element to the bottom of the deque (owner thread only).
     * 
     * Adds an element to the bottom end of the deque. This operation should
     * only be called by the owner thread. If the deque is at capacity, the
     * element is discarded.
     * 
     * @param item The item to move and store in the deque
     * @complexity O(1) - constant time insertion
     * @thread_safety Safe - but only one thread should call this method
     * @exception_safety Basic guarantee - if T's move constructor or operator new throws,
     *                  the deque remains unchanged
     * 
     * @note The element is moved into dynamically allocated storage.
     *       If capacity is reached, the element is discarded silently.
     *       Owner should check size() before pushing if capacity is a concern.
     */
    void push_bottom(T item) {
        T* element = new T(std::move(item));
        
        size_t bottom = bottom_.load(std::memory_order_relaxed);
        size_t top = top_.load(std::memory_order_acquire);
        
        // Check if full
        if (bottom - top >= CAPACITY - 1) {
            delete element;
            return; // Full, cannot push
        }
        
        // Store the element
        buffer_[bottom & INDEX_MASK].data.store(element, std::memory_order_relaxed);
        
        // Ensure the store is visible before updating bottom
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(bottom + 1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Pop an element from the bottom of the deque (owner thread only).
     * 
     * Removes and returns an element from the bottom end of the deque (LIFO behavior
     * for the owner). This provides good cache locality as the owner processes
     * its most recently added tasks first.
     * 
     * @return Pointer to the popped element, or nullptr if deque was empty.
     *         Caller is responsible for deleting the returned pointer.
     * @complexity O(1) - constant time removal
     * @thread_safety Safe - but only one thread should call this method
     * @exception_safety No-throw guarantee
     * 
     * @note Returns nullptr if deque is empty or if lost race with thief for last element.
     *       The returned pointer must be deleted by the caller to prevent memory leaks.
     */
    T* pop_bottom() {
        size_t bottom = bottom_.load(std::memory_order_relaxed);
        
        if (bottom == 0) {
            return nullptr; // Empty
        }
        
        // Decrement bottom first
        bottom = bottom - 1;
        bottom_.store(bottom, std::memory_order_relaxed);
        
        // Memory fence to ensure bottom update is seen by thieves
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        size_t top = top_.load(std::memory_order_relaxed);
        
        if (top <= bottom) {
            // Non-empty - we have at least one element
            T* element = buffer_[bottom & INDEX_MASK].data.exchange(nullptr, std::memory_order_relaxed);
            
            if (top == bottom) {
                // Last element - need to compete with thieves
                size_t expected_top = top;
                if (!top_.compare_exchange_strong(expected_top, top + 1,
                                                 std::memory_order_seq_cst,
                                                 std::memory_order_relaxed)) {
                    // Lost race to thief - restore bottom and return null
                    bottom_.store(bottom + 1, std::memory_order_relaxed);
                    return nullptr;
                }
                bottom_.store(bottom + 1, std::memory_order_relaxed);
            }
            
            return element;
        } else {
            // Empty - restore bottom
            bottom_.store(bottom + 1, std::memory_order_relaxed);
            return nullptr;
        }
    }
    
    /**
     * @brief Steal an element from the top of the deque (thief threads).
     * 
     * Removes and returns an element from the top end of the deque (FIFO behavior
     * relative to the owner's pushes). This provides good load balancing as
     * thieves steal the oldest tasks first.
     * 
     * @return Pointer to the stolen element, or nullptr if deque was empty or
     *         steal attempt failed. Caller is responsible for deleting the returned pointer.
     * @complexity O(1) - constant time removal attempt
     * @thread_safety Safe - multiple threads can call this concurrently
     * @exception_safety No-throw guarantee
     * 
     * @note May return nullptr even if deque appears non-empty due to races with
     *       other thieves or the owner. This is normal behavior in work-stealing.
     *       The returned pointer must be deleted by the caller to prevent memory leaks.
     */
    T* steal() {
        size_t top = top_.load(std::memory_order_acquire);
        
        // Memory fence to ensure we see the latest bottom
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        size_t bottom = bottom_.load(std::memory_order_acquire);
        
        if (top < bottom) {
            // Non-empty, try to steal the element
            T* element = buffer_[top & INDEX_MASK].data.load(std::memory_order_relaxed);
            
            if (element != nullptr) {
                // Try to claim this position by advancing top
                size_t expected_top = top;
                if (top_.compare_exchange_strong(expected_top, top + 1,
                                                std::memory_order_seq_cst,
                                                std::memory_order_relaxed)) {
                    // Successfully advanced top, now try to get the element
                    T* stolen = buffer_[top & INDEX_MASK].data.exchange(nullptr, std::memory_order_relaxed);
                    return stolen; // May be null if owner took it
                }
            }
        }
        
        return nullptr; // Failed to steal or empty
    }
    
    /**
     * @brief Check if the deque is empty.
     * 
     * @return true if the deque contains no elements, false otherwise
     * @complexity O(1) - constant time check
     * @thread_safety Safe - can be called from any thread
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment.
     *       This is an approximate check due to the nature of concurrent access.
     */
    bool empty() const {
        size_t top = top_.load(std::memory_order_acquire);
        size_t bottom = bottom_.load(std::memory_order_acquire);
        return top >= bottom;
    }
    
    /**
     * @brief Get the approximate size of the deque.
     * 
     * @return The approximate number of elements currently in the deque
     * @complexity O(1) - constant time calculation
     * @thread_safety Safe - can be called from any thread
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment.
     *       The size is approximate due to concurrent operations and should
     *       be used for informational purposes only.
     */
    size_t size() const {
        size_t bottom = bottom_.load(std::memory_order_acquire);
        size_t top = top_.load(std::memory_order_acquire);
        return (bottom >= top) ? (bottom - top) : 0;
    }
    
    /**
     * @brief Get the maximum capacity of the deque.
     * 
     * @return The maximum number of elements the deque can hold
     * @complexity O(1) - compile-time constant
     * @thread_safety Safe - static function
     * @exception_safety No-throw guarantee
     */
    static constexpr size_t capacity() {
        return CAPACITY - 1;
    }
};

} // namespace lockfree 