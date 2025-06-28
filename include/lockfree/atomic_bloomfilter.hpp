#pragma once

#include <atomic>
#include <memory>
#include <array>
#include <functional>
#include <cmath>
#include <vector>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe Bloom filter implementation.
 * 
 * A Bloom filter is a space-efficient probabilistic data structure designed to test
 * whether an element is a member of a set. False positive matches are possible, but
 * false negatives are not. This implementation uses atomic operations to ensure
 * thread safety without traditional locking mechanisms.
 * 
 * @tparam T The type of elements to test for membership. Must be hashable.
 * @tparam Size The total number of bits in the filter. Must be a power of 2.
 * @tparam NumHashFunctions The number of hash functions to use (1-8). More functions
 *                          reduce false positives but increase computation and memory access.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Probabilistic: May return false positives, but never false negatives
 * - Fixed memory: Constant memory usage regardless of elements inserted
 * - Multiple hash functions: Configurable number of hash functions for tuning
 * - Statistical analysis: Built-in false positive probability estimation
 * 
 * Performance Characteristics:
 * - Insert: O(k) where k is the number of hash functions
 * - Contains: O(k) where k is the number of hash functions
 * - Memory: O(Size/8) bytes of storage
 * - Space efficiency: Much more compact than explicit sets for large datasets
 * 
 * Algorithm Details:
 * - Uses bit array with atomic 64-bit words for thread-safe bit manipulation
 * - Multiple hash functions generated from single hash with different seeds
 * - Power-of-2 size enables efficient bit indexing with mask operations
 * - Atomic fetch_or operations ensure thread-safe bit setting
 * 
 * Mathematical Properties:
 * - False positive probability: ≈ (1 - e^(-kn/m))^k
 * - Optimal hash functions: k = (m/n) * ln(2)
 * - Where k = hash functions, n = elements, m = total bits
 * 
 * Usage Example:
 * @code
 * // Create filter for ~1000 elements with 3 hash functions
 * lockfree::AtomicBloomFilter<std::string, 8192, 3> filter;
 * 
 * // Insert elements
 * filter.insert("hello");
 * filter.insert("world");
 * 
 * // Test membership
 * if (filter.contains("hello")) {
 *     std::cout << "hello might be in the set" << std::endl;
 * }
 * 
 * if (!filter.contains("goodbye")) {
 *     std::cout << "goodbye is definitely not in the set" << std::endl;
 * }
 * 
 * // Get statistics
 * auto stats = filter.get_statistics();
 * std::cout << "False positive rate: " << stats.false_positive_probability << std::endl;
 * @endcode
 * 
 * @note Bloom filters cannot remove elements. Use counting Bloom filters if deletion is needed.
 * @warning False positives are possible. Always verify positive results with authoritative source.
 */
template<typename T, size_t Size = 8192, size_t NumHashFunctions = 3>
class AtomicBloomFilter {
private:
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    static_assert(NumHashFunctions > 0 && NumHashFunctions <= 8, "NumHashFunctions must be between 1 and 8");
    
    using BitArray = std::array<std::atomic<uint64_t>, Size / 64>;  ///< Atomic bit array type
    
    BitArray bits_;                             ///< Atomic bit array for the filter
    std::atomic<size_t> approximate_count_;     ///< Approximate count of unique insertions
    std::hash<T> hasher_;                       ///< Hash function for type T
    
    /**
     * @brief Pre-computed hash seeds for generating multiple independent hash functions.
     * 
     * These seeds are used to create k independent hash functions from a single
     * base hash value, following the double hashing technique.
     */
    static constexpr std::array<uint64_t, 8> hash_seeds_ = {
        0x9e3779b9, 0x85ebca6b, 0xc2b2ae35, 0x27d4eb2f,
        0x165667b1, 0xd3a2646c, 0xfd7046c5, 0xb55a4f09
    };
    
    static constexpr size_t BITS_PER_WORD = 64;         ///< Number of bits per atomic word
    static constexpr size_t WORD_COUNT = Size / BITS_PER_WORD;  ///< Number of atomic words
    static constexpr size_t BIT_MASK = Size - 1;        ///< Bit mask for efficient modulo operation
    
    /**
     * @brief Generate multiple hash values for an item using different seeds.
     * 
     * Creates k independent hash functions by combining the base hash with
     * different predetermined seeds. This approach is more efficient than
     * computing k completely independent hash functions.
     * 
     * @param item The item to hash
     * @return Array of k hash values for the item
     */
    std::array<size_t, NumHashFunctions> get_hash_values(const T& item) const {
        std::array<size_t, NumHashFunctions> hashes;
        size_t base_hash = hasher_(item);
        
        for (size_t i = 0; i < NumHashFunctions; ++i) {
            // Use different seeds to generate independent hash functions
            hashes[i] = (base_hash ^ hash_seeds_[i]) & BIT_MASK;
        }
        
        return hashes;
    }
    
    /**
     * @brief Set a bit atomically and return whether it was previously set.
     * 
     * Uses atomic fetch_or operation to set the bit while retrieving the
     * previous value, enabling detection of whether this was a new bit.
     * 
     * @param bit_index The index of the bit to set (0 to Size-1)
     * @return true if the bit was already set, false if it was newly set
     */
    bool set_bit(size_t bit_index) {
        size_t word_index = bit_index / BITS_PER_WORD;
        size_t bit_offset = bit_index % BITS_PER_WORD;
        uint64_t bit_mask = 1ULL << bit_offset;
        
        uint64_t old_value = bits_[word_index].fetch_or(bit_mask, std::memory_order_relaxed);
        return (old_value & bit_mask) != 0;
    }
    
    /**
     * @brief Check if a bit is set without modifying it.
     * 
     * @param bit_index The index of the bit to check (0 to Size-1)
     * @return true if the bit is set, false otherwise
     */
    bool is_bit_set(size_t bit_index) const {
        size_t word_index = bit_index / BITS_PER_WORD;
        size_t bit_offset = bit_index % BITS_PER_WORD;
        uint64_t bit_mask = 1ULL << bit_offset;
        
        uint64_t word_value = bits_[word_index].load(std::memory_order_relaxed);
        return (word_value & bit_mask) != 0;
    }
    
public:
    /**
     * @brief Default constructor. Creates an empty Bloom filter.
     * 
     * @complexity O(Size/64) - initializes all atomic words
     * @thread_safety Safe
     */
    AtomicBloomFilter() : approximate_count_(0) {
        clear();
    }
    
    /**
     * @brief Destructor.
     * 
     * @complexity O(1)
     * @thread_safety Not safe - should only be called when no other threads are accessing
     */
    ~AtomicBloomFilter() = default;
    
    // Non-copyable and non-movable due to complex atomic state
    AtomicBloomFilter(const AtomicBloomFilter&) = delete;
    AtomicBloomFilter& operator=(const AtomicBloomFilter&) = delete;
    AtomicBloomFilter(AtomicBloomFilter&&) = delete;  // Complex atomic state
    AtomicBloomFilter& operator=(AtomicBloomFilter&&) = delete;
    
    /**
     * @brief Insert an item into the Bloom filter.
     * 
     * @param item The item to insert into the filter
     * @return true if this appears to be a new insertion (no false negatives),
     *         false if the item was likely already present (may be false positive)
     * @complexity O(k) where k is the number of hash functions
     * @thread_safety Safe for concurrent insertions and queries
     * @exception_safety No-throw guarantee
     * 
     * @note Return value indicates probable novelty, not definitive uniqueness
     */
    bool insert(const T& item) {
        auto hash_values = get_hash_values(item);
        bool was_already_present = true;
        
        // Set all bits for this item
        for (size_t hash_val : hash_values) {
            bool bit_was_set = set_bit(hash_val);
            if (!bit_was_set) {
                was_already_present = false;
            }
        }
        
        // If at least one bit was newly set, increment approximate count
        if (!was_already_present) {
            approximate_count_.fetch_add(1, std::memory_order_relaxed);
        }
        
        return !was_already_present;  // Return true if this is a new insertion
    }
    
    /**
     * @brief Test if an item might be in the Bloom filter.
     * 
     * @param item The item to test for membership
     * @return true if item might be present (possible false positive),
     *         false if item is definitely not present (no false negatives)
     * @complexity O(k) where k is the number of hash functions
     * @thread_safety Safe for concurrent use with insert operations
     * @exception_safety No-throw guarantee
     * 
     * @note False positives are possible, false negatives are impossible
     */
    bool contains(const T& item) const {
        auto hash_values = get_hash_values(item);
        
        // Check all bits for this item
        for (size_t hash_val : hash_values) {
            if (!is_bit_set(hash_val)) {
                return false;  // Definitely not present
            }
        }
        
        return true;  // Might be present
    }
    
    /**
     * @brief Alternative name for contains() that emphasizes probabilistic nature.
     * 
     * @param item The item to test for membership
     * @return true if item might be present, false if definitely not present
     * @complexity O(k) where k is the number of hash functions
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This is an alias for contains() that makes the probabilistic nature explicit
     */
    bool might_contain(const T& item) const {
        return contains(item);
    }
    
    /**
     * @brief Clear all bits in the Bloom filter.
     * 
     * @complexity O(Size/64) - clears all atomic words
     * @thread_safety Not safe with concurrent operations
     * @exception_safety No-throw guarantee
     * 
     * @warning This operation is not thread-safe with concurrent insert/contains operations
     */
    void clear() {
        for (auto& word : bits_) {
            word.store(0, std::memory_order_relaxed);
        }
        approximate_count_.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get approximate number of unique items inserted.
     * 
     * @return Approximate count of items that appeared to be new when inserted
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This is an approximation and may overcount due to hash collisions
     */
    size_t approximate_size() const {
        return approximate_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get the number of bits currently set in the filter.
     * 
     * @return Count of set bits across the entire bit array
     * @complexity O(Size/64) - counts bits in all atomic words
     * @thread_safety Safe but may be inconsistent during concurrent modifications
     * @exception_safety No-throw guarantee
     */
    size_t bits_set() const {
        size_t count = 0;
        for (const auto& word : bits_) {
            uint64_t value = word.load(std::memory_order_relaxed);
            count += __builtin_popcountll(value);  // Count set bits
        }
        return count;
    }
    
    /**
     * @brief Get the load factor (ratio of set bits to total bits).
     * 
     * @return Load factor as a value between 0.0 and 1.0
     * @complexity O(Size/64) - requires counting all set bits
     * @thread_safety Safe but may be inconsistent during concurrent modifications
     * @exception_safety No-throw guarantee
     */
    double load_factor() const {
        return static_cast<double>(bits_set()) / Size;
    }
    
    /**
     * @brief Calculate the current false positive probability estimate.
     * 
     * @return Estimated false positive probability based on current load factor
     * @complexity O(Size/64) - requires computing load factor
     * @thread_safety Safe but may be inconsistent during concurrent modifications
     * @exception_safety No-throw guarantee
     * 
     * @note Uses formula: (1 - e^(-k*load_factor))^k where k is number of hash functions
     */
    double false_positive_probability() const {
        double load = load_factor();
        if (load >= 1.0) return 1.0;
        
        // Formula: (1 - e^(-k*n/m))^k
        // where k = number of hash functions, n = number of items, m = number of bits
        double exp_part = std::exp(-static_cast<double>(NumHashFunctions) * load);
        return std::pow(1.0 - exp_part, static_cast<double>(NumHashFunctions));
    }
    
    /**
     * @brief Statistics structure containing comprehensive filter information.
     */
    struct Statistics {
        size_t total_bits;                    ///< Total number of bits in the filter
        size_t bits_set;                     ///< Number of bits currently set
        size_t approximate_items;            ///< Approximate number of items inserted
        size_t hash_functions;               ///< Number of hash functions used
        double load_factor;                  ///< Ratio of set bits to total bits
        double false_positive_probability;   ///< Current false positive probability
    };
    
    /**
     * @brief Get comprehensive statistics about the filter state.
     * 
     * @return Statistics structure with current filter metrics
     * @complexity O(Size/64) - requires counting set bits
     * @thread_safety Safe but may be inconsistent during concurrent modifications
     * @exception_safety No-throw guarantee
     */
    Statistics get_statistics() const {
        return Statistics{
            .total_bits = Size,
            .bits_set = bits_set(),
            .approximate_items = approximate_size(),
            .hash_functions = NumHashFunctions,
            .load_factor = load_factor(),
            .false_positive_probability = false_positive_probability()
        };
    }
    
    /**
     * @brief Get the total capacity of the filter in bits.
     * 
     * @return The compile-time size of the bit array
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    static constexpr size_t capacity() {
        return Size;
    }
    
    /**
     * @brief Get the number of hash functions used.
     * 
     * @return The compile-time number of hash functions
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    static constexpr size_t num_hash_functions() {
        return NumHashFunctions;
    }
    
    /**
     * @brief Calculate optimal number of hash functions for expected elements.
     * 
     * @param expected_elements The expected number of elements to insert
     * @return Optimal number of hash functions (clamped to 1-8 range)
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Uses formula: k = (m/n) * ln(2) where m=bits, n=elements
     */
    static constexpr size_t optimal_hash_functions(size_t expected_elements) {
        if (expected_elements == 0) return 1;
        // Optimal k = (m/n) * ln(2)
        double optimal_k = (static_cast<double>(Size) / expected_elements) * std::log(2.0);
        return std::max(1UL, std::min(8UL, static_cast<size_t>(std::round(optimal_k))));
    }
    
    /**
     * @brief Calculate expected false positive rate for given number of elements.
     * 
     * @param num_elements The number of elements that will be inserted
     * @return Expected false positive probability
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Assumes uniform distribution of hash values and optimal loading
     */
    static double expected_false_positive_rate(size_t num_elements) {
        if (num_elements == 0) return 0.0;
        if (num_elements * NumHashFunctions >= Size) return 1.0;
        
        // Calculate expected load factor
        double load = static_cast<double>(num_elements * NumHashFunctions) / Size;
        double exp_part = std::exp(-load);
        return std::pow(1.0 - exp_part, static_cast<double>(NumHashFunctions));
    }
    
    /**
     * @brief Check if the filter is empty (no bits set).
     * 
     * @return true if no bits are set in the filter, false otherwise
     * @complexity O(Size/64) - checks all atomic words
     * @thread_safety Safe but may be inconsistent during concurrent modifications
     * @exception_safety No-throw guarantee
     */
    bool empty() const {
        return bits_set() == 0;
    }
    
    // Note: Union and intersection operations are not provided in the lock-free version
    // as they would require complex synchronization to maintain consistency.
    // Applications can implement these operations at a higher level by coordinating
    // insertions across multiple filters if needed.
};

// Template deduction guides and type aliases
template<typename T>
using AtomicBloomFilter8K = AtomicBloomFilter<T, 8192, 3>;

template<typename T>
using AtomicBloomFilter64K = AtomicBloomFilter<T, 65536, 4>;

template<typename T>
using AtomicBloomFilter1M = AtomicBloomFilter<T, 1048576, 5>;

using AtomicStringBloomFilter = AtomicBloomFilter<std::string>;
using AtomicIntBloomFilter = AtomicBloomFilter<int>;

} // namespace lockfree 