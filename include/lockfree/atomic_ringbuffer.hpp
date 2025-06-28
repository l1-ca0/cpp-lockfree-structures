#pragma once

#include <atomic>
#include <memory>
#include <array>
#include <utility>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe ring buffer (circular buffer) implementation.
 * 
 * This ring buffer provides fixed-capacity FIFO semantics without using traditional
 * locking mechanisms. It uses a circular array with atomic head and tail pointers to
 * ensure thread safety and lock-free progress. The buffer is particularly well-suited
 * for producer-consumer scenarios with bounded memory usage.
 * 
 * @tparam T The type of elements stored in the ring buffer. Must be constructible,
 *           destructible, and either copyable or movable.
 * @tparam Size The fixed capacity of the ring buffer. Must be a power of 2 and greater than 1.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Fixed capacity: Bounded memory usage with compile-time size
 * - Exception-safe: Basic exception safety guarantee
 * - Move semantics: Efficient for move-only and expensive-to-copy types
 * - SPSC optimizations: Special single-producer single-consumer variants for maximum performance
 * 
 * Performance Characteristics:
 * - Push: O(1) amortized, may retry under contention
 * - Pop: O(1) amortized, may retry under contention
 * - Memory: O(Size) - fixed memory footprint
 * - Cache-friendly: Uses cache-line alignment to reduce false sharing
 * 
 * Algorithm Details:
 * - Uses separate atomic head and tail counters for safe concurrent access
 * - Each slot stores a pointer to dynamically allocated data
 * - Power-of-2 size requirement enables efficient bit-mask indexing
 * - Cache-aligned structures minimize false sharing between threads
 * 
 * Memory Management:
 * - Uses dynamic allocation for data elements (not the slots themselves)
 * - Data is allocated on push and freed on pop
 * - All memory is properly cleaned up in destructor
 * - Buffer capacity is fixed at compile time
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicRingBuffer<int, 1024> buffer;
 * 
 * // Producer thread
 * buffer.push(42);
 * buffer.emplace(100);
 * 
 * // Consumer thread
 * int value;
 * if (buffer.pop(value)) {
 *     std::cout << "Consumed: " << value << std::endl;
 * }
 * 
 * // High-performance single-producer single-consumer
 * buffer.spsc_push(99);
 * if (buffer.spsc_pop(value)) {
 *     std::cout << "SPSC consumed: " << value << std::endl;
 * }
 * @endcode
 * 
 * @note The size template parameter must be a power of 2 for optimal performance.
 * @warning This is a bounded container - operations fail when capacity is exceeded.
 */
template<typename T, size_t Size>
class AtomicRingBuffer {
private:
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    static_assert(Size > 1, "Size must be greater than 1");
    
    /**
     * @brief Internal slot structure for ring buffer elements.
     * 
     * Each slot contains a pointer to data and a validity flag.
     * Cache-line alignment prevents false sharing between adjacent slots.
     */
    struct Slot {
        alignas(64) std::atomic<T*> data{nullptr};  ///< Atomic pointer to stored data
        std::atomic<bool> valid{false};             ///< Atomic flag indicating if slot contains valid data
    };
    
    alignas(64) std::array<Slot, Size> buffer_;     ///< Fixed-size circular buffer array
    alignas(64) std::atomic<uint64_t> head_{0};     ///< Atomic head counter (producer index)
    alignas(64) std::atomic<uint64_t> tail_{0};     ///< Atomic tail counter (consumer index)
    alignas(64) std::atomic<size_t> size_{0};       ///< Atomic element count
    
    static constexpr size_t INDEX_MASK = Size - 1;  ///< Bit mask for efficient modulo operation
    
public:
    /**
     * @brief Default constructor. Creates an empty ring buffer.
     * 
     * @complexity O(Size) - initializes all slots
     * @thread_safety Safe
     */
    AtomicRingBuffer();
    
    /**
     * @brief Destructor. Cleans up any remaining elements.
     * 
     * @complexity O(n) where n is the number of remaining elements
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicRingBuffer();
    
    // Non-copyable but movable for resource management
    AtomicRingBuffer(const AtomicRingBuffer&) = delete;
    AtomicRingBuffer& operator=(const AtomicRingBuffer&) = delete;
    AtomicRingBuffer(AtomicRingBuffer&&) = default;
    AtomicRingBuffer& operator=(AtomicRingBuffer&&) = default;
    
    /**
     * @brief Push an element to the buffer by copying.
     * 
     * @param item The item to copy and insert into the buffer
     * @return true if successfully inserted, false if buffer is full
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other push/pop operations
     * @exception_safety Basic guarantee - if T's copy constructor throws,
     *                  the buffer remains unchanged
     */
    bool push(const T& item);
    
    /**
     * @brief Push an element to the buffer by moving.
     * 
     * @param item The item to move and insert into the buffer
     * @return true if successfully inserted, false if buffer is full
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other push/pop operations
     * @exception_safety Basic guarantee - if T's move constructor throws,
     *                  the buffer remains unchanged
     */
    bool push(T&& item);
    
    /**
     * @brief Construct an element in-place in the buffer.
     * 
     * @tparam Args Types of arguments for T's constructor
     * @param args Arguments to forward to T's constructor
     * @return true if successfully constructed and inserted, false if buffer is full
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other push/pop operations
     * @exception_safety Basic guarantee - if T's constructor throws,
     *                  the buffer remains unchanged
     */
    template<typename... Args>
    bool emplace(Args&&... args);
    
    /**
     * @brief Pop an element from the front of the buffer.
     * 
     * @param result Reference to store the popped element
     * @return true if an element was successfully popped, false if buffer was empty
     * @complexity O(1) amortized, may retry under high contention
     * @thread_safety Safe for concurrent use with other push/pop operations
     * @exception_safety Strong guarantee - if operation fails, buffer is unchanged
     * 
     * @note May fail and return false under extreme contention after 1000 retry attempts
     */
    bool pop(T& result);
    
    /**
     * @brief Peek at the front element without removing it.
     * 
     * @param result Reference to store a copy of the front element
     * @return true if front element was successfully read, false if buffer was empty
     * @complexity O(1)
     * @thread_safety Safe but with caveats (see warning)
     * @exception_safety Basic guarantee
     * 
     * @warning This operation has race condition possibilities. The element might
     *          be consumed by another thread between reading the pointer and
     *          accessing the data. Use with caution in high-contention scenarios.
     */
    bool front(T& result) const;
    
    /**
     * @brief Peek at the back element without removing it.
     * 
     * @param result Reference to store a copy of the back element
     * @return true if back element was successfully read, false if buffer was empty
     * @complexity O(1)
     * @thread_safety Safe but with caveats (see warning)
     * @exception_safety Basic guarantee
     * 
     * @warning This operation has race condition possibilities. The element might
     *          be modified by another thread between reading the pointer and
     *          accessing the data. Use with caution in high-contention scenarios.
     */
    bool back(T& result) const;
    
    /**
     * @brief Check if the buffer is empty.
     * 
     * @return true if the buffer contains no elements, false otherwise
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment
     */
    bool empty() const;
    
    /**
     * @brief Check if the buffer is full.
     * 
     * @return true if the buffer has reached its capacity, false otherwise
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment
     */
    bool full() const;
    
    /**
     * @brief Get the current number of elements in the buffer.
     * 
     * @return The number of elements currently in the buffer
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment
     */
    size_t size() const;
    
    /**
     * @brief Get the maximum number of elements the buffer can hold.
     * 
     * @return The compile-time capacity of the buffer
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    constexpr size_t capacity() const;
    
    /**
     * @brief High-performance push for single-producer single-consumer scenarios.
     * 
     * @param item The item to copy and insert into the buffer
     * @return true if successfully inserted, false if buffer is full
     * @complexity O(1) - optimized for single producer
     * @thread_safety Only safe when called by a single producer thread
     *                with spsc_pop called by a single consumer thread
     * @exception_safety Basic guarantee
     * 
     * @warning This method assumes single-producer usage. Using it with multiple
     *          producer threads will result in undefined behavior.
     */
    bool spsc_push(const T& item);
    
    /**
     * @brief High-performance push for single-producer single-consumer scenarios.
     * 
     * @param item The item to move and insert into the buffer
     * @return true if successfully inserted, false if buffer is full
     * @complexity O(1) - optimized for single producer
     * @thread_safety Only safe when called by a single producer thread
     *                with spsc_pop called by a single consumer thread
     * @exception_safety Basic guarantee
     * 
     * @warning This method assumes single-producer usage. Using it with multiple
     *          producer threads will result in undefined behavior.
     */
    bool spsc_push(T&& item);
    
    /**
     * @brief High-performance pop for single-producer single-consumer scenarios.
     * 
     * @param result Reference to store the popped element
     * @return true if an element was successfully popped, false if buffer was empty
     * @complexity O(1) - optimized for single consumer
     * @thread_safety Only safe when called by a single consumer thread
     *                with spsc_push called by a single producer thread
     * @exception_safety Strong guarantee
     * 
     * @warning This method assumes single-consumer usage. Using it with multiple
     *          consumer threads will result in undefined behavior.
     */
    bool spsc_pop(T& result);
    
private:
    /**
     * @brief Internal implementation for push operations.
     * 
     * @param item Pointer to the item to insert (takes ownership)
     * @return true if successfully inserted, false if buffer is full
     */
    bool push_impl(T* item);
    
    /**
     * @brief Internal implementation for pop operations.
     * 
     * @param item Reference to pointer that will receive the popped item
     * @return true if successfully popped, false if buffer was empty
     */
    bool pop_impl(T*& item);
};

template<typename T, size_t Size>
AtomicRingBuffer<T, Size>::AtomicRingBuffer() {
    // Initialize all slots as invalid/empty
    for (size_t i = 0; i < Size; ++i) {
        buffer_[i].valid.store(false, std::memory_order_relaxed);
        buffer_[i].data.store(nullptr, std::memory_order_relaxed);
    }
}

template<typename T, size_t Size>
AtomicRingBuffer<T, Size>::~AtomicRingBuffer() {
    T item;
    while (pop(item)) {
        // Clean up remaining items
    }
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::push(const T& item) {
    return push_impl(new T(item));
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::push(T&& item) {
    return push_impl(new T(std::move(item)));
}

template<typename T, size_t Size>
template<typename... Args>
bool AtomicRingBuffer<T, Size>::emplace(Args&&... args) {
    return push_impl(new T(std::forward<Args>(args)...));
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::push_impl(T* item) {
    // Check if buffer is full
    if (size_.load(std::memory_order_acquire) >= Size) {
        delete item;
        return false;
    }
    
    int attempts = 0;
    while (attempts < 1000) {  // Bounded retry
        uint64_t current_head = head_.load(std::memory_order_acquire);
        uint64_t next_head = current_head + 1;
        
        // Check if this would make us full
        uint64_t current_tail = tail_.load(std::memory_order_acquire);
        if ((next_head - current_tail) > Size) {
            delete item;
            return false;
        }
        
        Slot& slot = buffer_[current_head & INDEX_MASK];
        
        // Check if slot is available
        bool expected_valid = false;
        if (slot.valid.load(std::memory_order_acquire) == false) {
            // Try to claim this slot
            if (head_.compare_exchange_weak(current_head, next_head,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
                // Successfully claimed slot
                slot.data.store(item, std::memory_order_release);
                slot.valid.store(true, std::memory_order_release);
                size_.fetch_add(1, std::memory_order_acq_rel);
                return true;
            }
        }
        attempts++;
    }
    
    delete item;
    return false;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::pop(T& result) {
    T* item;
    if (pop_impl(item)) {
        result = std::move(*item);
        delete item;
        return true;
    }
    return false;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::pop_impl(T*& item) {
    // Check if buffer is empty
    if (size_.load(std::memory_order_acquire) == 0) {
        return false;
    }
    
    int attempts = 0;
    while (attempts < 1000) {  // Bounded retry
        uint64_t current_tail = tail_.load(std::memory_order_acquire);
        uint64_t next_tail = current_tail + 1;
        
        Slot& slot = buffer_[current_tail & INDEX_MASK];
        
        // Check if slot has data
        if (slot.valid.load(std::memory_order_acquire)) {
            // Try to claim this slot
            if (tail_.compare_exchange_weak(current_tail, next_tail,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
                // Successfully claimed slot - atomically extract the pointer
                item = slot.data.exchange(nullptr, std::memory_order_acq_rel);
                slot.valid.store(false, std::memory_order_release);
                size_.fetch_sub(1, std::memory_order_acq_rel);
                
                // Verify we got a valid pointer
                if (item == nullptr) {
                    // Race condition occurred, slot was already consumed
                    return false;
                }
                return true;
            }
        } else {
            return false;  // No data available
        }
        attempts++;
    }
    
    return false;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::front(T& result) const {
    uint64_t current_tail = tail_.load(std::memory_order_acquire);
    const Slot& slot = buffer_[current_tail & INDEX_MASK];
    
    if (slot.valid.load(std::memory_order_acquire)) {
        T* item = slot.data.load(std::memory_order_acquire);
        if (item) {
            result = *item;
            return true;
        }
    }
    
    return false;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::back(T& result) const {
    uint64_t current_head = head_.load(std::memory_order_acquire);
    if (current_head == 0) return false;
    
    const Slot& slot = buffer_[(current_head - 1) & INDEX_MASK];
    
    if (slot.valid.load(std::memory_order_acquire)) {
        T* item = slot.data.load(std::memory_order_acquire);
        if (item) {
            result = *item;
            return true;
        }
    }
    
    return false;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::empty() const {
    return size_.load(std::memory_order_acquire) == 0;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::full() const {
    return size_.load(std::memory_order_acquire) >= Size;
}

template<typename T, size_t Size>
size_t AtomicRingBuffer<T, Size>::size() const {
    return size_.load(std::memory_order_acquire);
}

template<typename T, size_t Size>
constexpr size_t AtomicRingBuffer<T, Size>::capacity() const {
    return Size;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::spsc_push(const T& item) {
    uint64_t current_head = head_.load(std::memory_order_relaxed);
    uint64_t next_head = current_head + 1;
    
    // Check if buffer is full (SPSC optimized)
    uint64_t current_tail = tail_.load(std::memory_order_acquire);
    if ((next_head - current_tail) > Size) {
        return false;
    }
    
    Slot& slot = buffer_[current_head & INDEX_MASK];
    T* new_item = new T(item);
    
    slot.data.store(new_item, std::memory_order_relaxed);
    slot.valid.store(true, std::memory_order_release);
    head_.store(next_head, std::memory_order_relaxed);
    
    return true;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::spsc_push(T&& item) {
    uint64_t current_head = head_.load(std::memory_order_relaxed);
    uint64_t next_head = current_head + 1;
    
    // Check if buffer is full (SPSC optimized)
    uint64_t current_tail = tail_.load(std::memory_order_acquire);
    if ((next_head - current_tail) > Size) {
        return false;
    }
    
    Slot& slot = buffer_[current_head & INDEX_MASK];
    T* new_item = new T(std::move(item));
    
    slot.data.store(new_item, std::memory_order_relaxed);
    slot.valid.store(true, std::memory_order_release);
    head_.store(next_head, std::memory_order_relaxed);
    
    return true;
}

template<typename T, size_t Size>
bool AtomicRingBuffer<T, Size>::spsc_pop(T& result) {
    uint64_t current_tail = tail_.load(std::memory_order_relaxed);
    Slot& slot = buffer_[current_tail & INDEX_MASK];
    
    // Check if slot has data (SPSC optimized)
    if (!slot.valid.load(std::memory_order_acquire)) {
        return false;
    }
    
    T* item = slot.data.load(std::memory_order_relaxed);
    result = std::move(*item);
    delete item;
    
    slot.data.store(nullptr, std::memory_order_relaxed);
    slot.valid.store(false, std::memory_order_release);
    tail_.store(current_tail + 1, std::memory_order_relaxed);
    
    return true;
}

} // namespace lockfree