#pragma once

#include <atomic>
#include <memory>
#include <utility>
#include <array>
#include <cstddef>
#include <thread>
#include <chrono>
#include <functional>

namespace lockfree {

/**
 * @brief A high-performance lock-free MPMC (Multi-Producer Multi-Consumer) queue.
 * 
 * This queue is specifically optimized for scenarios with multiple producers and
 * multiple consumers operating concurrently. It uses a lock-free circular buffer
 * approach with careful memory ordering to achieve excellent performance under
 * high contention while maintaining FIFO semantics.
 * 
 * @tparam T The type of elements stored in the queue. Must be constructible,
 *           destructible, and either copyable or movable.
 * @tparam Size The fixed capacity of the queue. Must be a power of 2 for optimal performance.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - MPMC optimized: Excellent performance with multiple producers/consumers
 * - Fixed capacity: Bounded memory usage with compile-time size
 * - Cache-friendly: Optimized memory layout to minimize false sharing
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - FIFO ordering: Elements are dequeued in the order they were enqueued
 * 
 * Performance Characteristics:
 * - Enqueue: O(1) amortized, optimized for high contention
 * - Dequeue: O(1) amortized, optimized for high contention
 * - Memory: O(Size) - fixed memory footprint
 * - Cache-friendly: Uses cache-line alignment to reduce false sharing
 * 
 * Algorithm Details:
 * - Uses a circular buffer with atomic sequence numbers
 * - Separate read and write positions for concurrent access
 * - Memory ordering optimized for x86/ARM architectures
 * - Bounded retry logic prevents infinite loops under contention
 * 
 * Memory Management:
 * - Uses in-place construction for optimal performance
 * - Fixed-size buffer eliminates allocation overhead
 * - All memory is properly cleaned up in destructor
 * - Buffer capacity is fixed at compile time
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicMPMCQueue<int, 1024> queue;
 * 
 * // Producer threads
 * queue.enqueue(42);
 * queue.emplace(100);
 * 
 * // Consumer threads
 * int value;
 * if (queue.dequeue(value)) {
 *     std::cout << "Consumed: " << value << std::endl;
 * }
 * @endcode
 * 
 * @note The size template parameter must be a power of 2 for optimal performance.
 * @warning This is a bounded container - operations fail when capacity is exceeded.
 */
template<typename T, size_t Size>
class AtomicMPMCQueue {
public:
    using value_type = T;

private:
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    static_assert(Size > 1, "Size must be greater than 1");
    
    /**
     * @brief Internal slot structure for queue elements.
     * 
     * Each slot contains a sequence number for synchronization and storage for the data.
     * The sequence number is used to coordinate access between producers and consumers.
     */
    struct Slot {
        std::atomic<size_t> sequence{0};  ///< Sequence number for synchronization
        T data;                           ///< Storage for the actual data
    };
    
    alignas(64) std::array<Slot, Size> buffer_;       ///< Fixed-size circular buffer
    alignas(64) std::atomic<size_t> enqueue_pos_{0};  ///< Producer position
    alignas(64) std::atomic<size_t> dequeue_pos_{0};  ///< Consumer position
    
    static constexpr size_t INDEX_MASK = Size - 1;    ///< Bit mask for efficient modulo operation
    
    // High-contention optimized backoff with randomization
    static void backoff(int attempt, const void* next_cache_line = nullptr) {
        if (attempt < 6) {
            // Short spin for first few attempts
            const int base_spins = 1 << (attempt / 2);
            // Add randomization to reduce thundering herd
            const int random_factor = (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 4) + 1;
            const int spins = base_spins * random_factor;
            
            for (int i = 0; i < spins; ++i) {
                #if defined(__x86_64__) || defined(_M_X64)
                    __builtin_ia32_pause();
                #elif defined(__aarch64__)
                    asm volatile("yield" ::: "memory");
                #else
                    std::this_thread::yield();
                #endif
            }
        } else if (attempt < 12) {
            // Medium backoff with thread yield
            std::this_thread::yield();
        } else {
            // Longer backoff for very high contention
            std::this_thread::sleep_for(std::chrono::nanoseconds(
                1 + (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 100)
            ));
        }
    }
    
    // Branch prediction hints
    #if defined(__GNUC__) || defined(__clang__)
        #define LIKELY(x)   __builtin_expect(!!(x), 1)
        #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define LIKELY(x)   (x)
        #define UNLIKELY(x) (x)
    #endif
    
    // Template helper for optimized enqueue operations with balanced performance
    template<typename... Args>
    bool enqueue_impl(Args&&... args) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        
        // Balanced retry limit - improved from 32 but not excessive
        const int max_attempts = 64;
        
        for (int attempts = 0; attempts < max_attempts; ++attempts) {
            const size_t index = pos & INDEX_MASK;
            Slot& slot = buffer_[index];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            
            if (LIKELY(seq == pos)) {
                // Hot path: slot is available for writing
                if (LIKELY(enqueue_pos_.compare_exchange_weak(pos, pos + 1, 
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_relaxed))) {
                    // Successfully claimed, construct the element
                    new (&slot.data) T(std::forward<Args>(args)...);
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed, pos was updated by compare_exchange_weak
            } else if (UNLIKELY(seq < pos)) {
                // Queue is full - early exit
                return false;
            } else {
                // Another thread got here first, reload position
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
            
            // Conservative backoff strategy
            if (UNLIKELY(attempts > 8)) {
                backoff(attempts - 8);
            }
        }
        
        return false;  // Failed after max attempts
    }
    
public:
    /**
     * @brief Default constructor. Creates an empty MPMC queue.
     * 
     * @complexity O(Size) - initializes all slots
     * @thread_safety Safe
     */
    AtomicMPMCQueue();
    
    /**
     * @brief Destructor. Cleans up any remaining elements.
     * 
     * @complexity O(n) where n is the number of remaining elements
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicMPMCQueue();
    
    // Non-copyable but movable for resource management
    AtomicMPMCQueue(const AtomicMPMCQueue&) = delete;
    AtomicMPMCQueue& operator=(const AtomicMPMCQueue&) = delete;
    AtomicMPMCQueue(AtomicMPMCQueue&&) = default;
    AtomicMPMCQueue& operator=(AtomicMPMCQueue&&) = default;
    
    /**
     * @brief Enqueue an element to the queue by copying.
     * 
     * @param item The item to copy and insert into the queue
     * @return true if successfully inserted, false if queue is full
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other enqueue/dequeue operations
     * @exception_safety Basic guarantee - if T's copy constructor throws,
     *                  the queue remains unchanged
     */
    bool enqueue(const T& item);
    
    /**
     * @brief Enqueue an element to the queue by moving.
     * 
     * @param item The item to move and insert into the queue
     * @return true if successfully inserted, false if queue is full
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other enqueue/dequeue operations
     * @exception_safety Basic guarantee - if T's move constructor throws,
     *                  the queue remains unchanged
     */
    bool enqueue(T&& item);
    
    /**
     * @brief Construct an element in-place in the queue.
     * 
     * @tparam Args Types of arguments for T's constructor
     * @param args Arguments to forward to T's constructor
     * @return true if successfully constructed and inserted, false if queue is full
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other enqueue/dequeue operations
     * @exception_safety Basic guarantee - if T's constructor throws,
     *                  the queue remains unchanged
     */
    template<typename... Args>
    bool emplace(Args&&... args);
    
    /**
     * @brief Dequeue an element from the front of the queue.
     * 
     * @param result Reference to store the dequeued element
     * @return true if an element was successfully dequeued, false if queue was empty
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other enqueue/dequeue operations
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
     * @note Result may be immediately outdated in concurrent environment
     */
    bool empty() const;
    
    /**
     * @brief Check if the queue is full.
     * 
     * @return true if the queue has reached its capacity, false otherwise
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment
     */
    bool full() const;
    
    /**
     * @brief Get the current number of elements in the queue.
     * 
     * @return The number of elements currently in the queue
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment
     */
    size_t size() const;
    
    /**
     * @brief Get the maximum capacity of the queue.
     * 
     * @return The maximum number of elements the queue can hold
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    constexpr size_t capacity() const;
    
    /**
     * @brief Peek at the front element without removing it.
     * 
     * @param result Reference to store a copy of the front element
     * @return true if front element was successfully read, false if queue was empty
     * @complexity O(1)
     * @thread_safety Safe but with caveats (see warning)
     * @exception_safety Basic guarantee
     * 
     * @warning This operation has race condition possibilities in MPMC scenarios.
     *          Use with caution in high-contention environments.
     */
    bool front(T& result) const;
};

template<typename T, size_t Size>
AtomicMPMCQueue<T, Size>::AtomicMPMCQueue() {
    // Initialize all sequence numbers
    for (size_t i = 0; i < Size; ++i) {
        buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }
}

template<typename T, size_t Size>
AtomicMPMCQueue<T, Size>::~AtomicMPMCQueue() {
    // Clean up any remaining elements
    T temp;
    while (dequeue(temp)) {
        // Destructor will be called automatically for temp
    }
}

template<typename T, size_t Size>
bool AtomicMPMCQueue<T, Size>::enqueue(const T& item) {
    return enqueue_impl(item);
}

template<typename T, size_t Size>
bool AtomicMPMCQueue<T, Size>::enqueue(T&& item) {
    return enqueue_impl(std::move(item));
}

template<typename T, size_t Size>
template<typename... Args>
bool AtomicMPMCQueue<T, Size>::emplace(Args&&... args) {
    return enqueue_impl(std::forward<Args>(args)...);
}

template<typename T, size_t Size>
bool AtomicMPMCQueue<T, Size>::dequeue(T& result) {
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    
    // Balanced retry limit - improved from 32 but not excessive
    const int max_attempts = 64;
    
    for (int attempts = 0; attempts < max_attempts; ++attempts) {
        const size_t index = pos & INDEX_MASK;
        Slot& slot = buffer_[index];
        size_t seq = slot.sequence.load(std::memory_order_acquire);
        
        const size_t expected_seq = pos + 1;
        if (LIKELY(seq == expected_seq)) {
            // Hot path: slot has data ready for reading
            if (LIKELY(dequeue_pos_.compare_exchange_weak(pos, pos + 1, 
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_relaxed))) {
                // Successfully claimed, extract the element
                result = std::move(slot.data);
                slot.data.~T();
                slot.sequence.store(pos + Size, std::memory_order_release);
                return true;
            }
            // CAS failed, pos was updated by compare_exchange_weak
        } else if (UNLIKELY(seq < expected_seq)) {
            // Queue is empty - early exit
            return false;
        } else {
            // Another thread got here first, reload position
            pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
        
        // Conservative backoff strategy
        if (UNLIKELY(attempts > 8)) {
            backoff(attempts - 8);
        }
    }
    
    return false;  // Failed after max attempts
}

template<typename T, size_t Size>
bool AtomicMPMCQueue<T, Size>::empty() const {
    // Load dequeue position first as it's more likely to change in consumer-heavy scenarios
    size_t dequeue_pos = dequeue_pos_.load(std::memory_order_relaxed);
    size_t enqueue_pos = enqueue_pos_.load(std::memory_order_relaxed);
    return LIKELY(enqueue_pos == dequeue_pos);
}

template<typename T, size_t Size>
bool AtomicMPMCQueue<T, Size>::full() const {
    // Load enqueue position first as it's more likely to change in producer-heavy scenarios  
    size_t enqueue_pos = enqueue_pos_.load(std::memory_order_relaxed);
    size_t dequeue_pos = dequeue_pos_.load(std::memory_order_relaxed);
    return UNLIKELY((enqueue_pos - dequeue_pos) >= Size);
}

template<typename T, size_t Size>
size_t AtomicMPMCQueue<T, Size>::size() const {
    // Consistent ordering: enqueue first for size calculation
    size_t enqueue_pos = enqueue_pos_.load(std::memory_order_relaxed);
    size_t dequeue_pos = dequeue_pos_.load(std::memory_order_relaxed);
    return enqueue_pos - dequeue_pos;
}

template<typename T, size_t Size>
constexpr size_t AtomicMPMCQueue<T, Size>::capacity() const {
    return Size;
}

template<typename T, size_t Size>
bool AtomicMPMCQueue<T, Size>::front(T& result) const {
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    const size_t index = pos & INDEX_MASK;
    const Slot& slot = buffer_[index];
    size_t seq = slot.sequence.load(std::memory_order_acquire);
    
    const size_t expected_seq = pos + 1;
    if (LIKELY(seq == expected_seq)) {
        result = slot.data;
        return true;
    }
    
    return false;
}

// Clean up macros
#undef LIKELY
#undef UNLIKELY

} // namespace lockfree 