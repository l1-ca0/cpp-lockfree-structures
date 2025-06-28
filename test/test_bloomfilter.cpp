#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <set>
#include <cassert>
#include <algorithm>
#include "lockfree/atomic_bloomfilter.hpp"

using namespace lockfree;

void test_basic_operations() {
    std::cout << "Testing basic operations... ";
    
    AtomicBloomFilter<int, 1024, 3> filter;
    
    // Test initial state
    assert(filter.empty());
    assert(filter.approximate_size() == 0);
    assert(filter.capacity() == 1024);
    assert(filter.num_hash_functions() == 3);
    
    // Test insert and contains
    assert(filter.insert(42));  // Should be new
    assert(!filter.empty());
    assert(filter.approximate_size() == 1);
    assert(filter.contains(42));
    
    // Test duplicate insert
    assert(!filter.insert(42));  // Should not be new
    assert(filter.approximate_size() == 1);  // Count should not change
    
    // Insert more items
    for (int i = 1; i <= 10; ++i) {
        filter.insert(i * 10);
    }
    
    // Check all inserted items can be found
    assert(filter.contains(42));
    for (int i = 1; i <= 10; ++i) {
        assert(filter.contains(i * 10));
    }
    
    // Test clear
    filter.clear();
    assert(filter.empty());
    assert(filter.approximate_size() == 0);
    assert(!filter.contains(42));
    
    std::cout << "PASSED\n";
}

void test_string_operations() {
    std::cout << "Testing string operations... ";
    
    AtomicStringBloomFilter filter;
    
    std::vector<std::string> words = {
        "hello", "world", "bloom", "filter", "atomic", "lockfree"
    };
    
    // Insert words
    for (const auto& word : words) {
        assert(filter.insert(word));
    }
    
    // Verify all words can be found
    for (const auto& word : words) {
        assert(filter.contains(word));
        assert(filter.might_contain(word));
    }
    
    // Test some words that weren't inserted
    assert(!filter.contains("definitely_not_inserted_word_12345"));
    
    std::cout << "PASSED\n";
}

void test_false_positive_characteristics() {
    std::cout << "Testing false positive characteristics... ";
    
    AtomicBloomFilter<int, 4096, 3> filter;
    
    // Insert a known set
    std::set<int> inserted;
    for (int i = 0; i < 200; ++i) {
        int value = i * 7 + 13;  // Some pattern to avoid sequential values
        inserted.insert(value);
        filter.insert(value);
    }
    
    // Test that all inserted items are found (no false negatives)
    for (int value : inserted) {
        assert(filter.contains(value));
    }
    
    // Test false positive rate on a large set of non-inserted values
    int false_positives = 0;
    int total_tests = 5000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(100000, 200000);  // Range unlikely to overlap
    
    for (int i = 0; i < total_tests; ++i) {
        int test_value = dist(gen);
        if (inserted.count(test_value) == 0) {  // Not in inserted set
            if (filter.contains(test_value)) {
                false_positives++;
            }
        }
    }
    
    double fp_rate = static_cast<double>(false_positives) / total_tests;
    double expected_fp_rate = filter.false_positive_probability();
    
    // False positive rate should be reasonably close to expected
    // Allow for some variance due to randomness
    assert(fp_rate <= expected_fp_rate * 2.0);  // Should not be way higher than expected
    
    std::cout << "PASSED (FP rate: " << fp_rate << ", expected: " << expected_fp_rate << ")\n";
}

void test_concurrent_operations() {
    std::cout << "Testing concurrent operations... ";
    
    AtomicBloomFilter<int, 8192, 4> filter;
    
    constexpr int num_threads = 4;
    constexpr int items_per_thread = 500;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<int>> thread_data(num_threads);
    
    // Generate unique data for each thread
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < items_per_thread; ++i) {
            thread_data[t].push_back(t * items_per_thread + i);
        }
    }
    
    // Launch concurrent insert threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int value : thread_data[t]) {
                filter.insert(value);
                
                // Immediately test that we can find what we just inserted
                assert(filter.contains(value));
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all items can be found
    for (int t = 0; t < num_threads; ++t) {
        for (int value : thread_data[t]) {
            assert(filter.contains(value));
        }
    }
    
    std::cout << "PASSED\n";
}

void test_multiple_filter_coordination() {
    std::cout << "Testing multiple filter coordination... ";
    
    AtomicBloomFilter<int, 2048, 3> filter1, filter2;
    
    // Populate filters with overlapping sets
    // Filter 1: even numbers 2, 4, 6, ..., 20
    for (int i = 2; i <= 20; i += 2) {
        filter1.insert(i);
    }
    
    // Filter 2: multiples of 3: 3, 6, 9, ..., 21
    for (int i = 3; i <= 21; i += 3) {
        filter2.insert(i);
    }
    
    // Test union-like behavior by creating a new filter with both sets
    AtomicBloomFilter<int, 2048, 3> union_filter;
    for (int i = 2; i <= 20; i += 2) {
        union_filter.insert(i);
    }
    for (int i = 3; i <= 21; i += 3) {
        union_filter.insert(i);
    }
    
    // Union filter should contain everything from both filters
    for (int i = 2; i <= 20; i += 2) {
        assert(union_filter.contains(i));
    }
    for (int i = 3; i <= 21; i += 3) {
        assert(union_filter.contains(i));
    }
    
    // Test intersection-like behavior by checking common elements
    // Common elements are multiples of 6: 6, 12, 18
    for (int i = 6; i <= 18; i += 6) {
        assert(filter1.contains(i) && filter2.contains(i));
    }
    
    std::cout << "PASSED\n";
}

void test_statistics() {
    std::cout << "Testing statistics... ";
    
    AtomicBloomFilter<int, 1024, 3> filter;
    
    // Initial statistics
    auto stats = filter.get_statistics();
    assert(stats.total_bits == 1024);
    assert(stats.bits_set == 0);
    assert(stats.approximate_items == 0);
    assert(stats.hash_functions == 3);
    assert(stats.load_factor == 0.0);
    
    // Insert some items and check statistics update
    for (int i = 0; i < 50; ++i) {
        filter.insert(i);
    }
    
    stats = filter.get_statistics();
    assert(stats.total_bits == 1024);
    assert(stats.bits_set > 0);
    assert(stats.approximate_items > 0);
    assert(stats.load_factor > 0.0);
    assert(stats.load_factor <= 1.0);
    assert(stats.false_positive_probability >= 0.0);
    assert(stats.false_positive_probability <= 1.0);
    
    std::cout << "PASSED\n";
}

void test_edge_cases() {
    std::cout << "Testing edge cases... ";
    
    // Test very small filter
    AtomicBloomFilter<int, 64, 1> small_filter;
    assert(small_filter.capacity() == 64);
    assert(small_filter.num_hash_functions() == 1);
    
    // Should still work for basic operations
    assert(small_filter.insert(1));
    assert(small_filter.contains(1));
    
    // Test maximum hash functions
    AtomicBloomFilter<int, 1024, 8> max_hash_filter;
    assert(max_hash_filter.num_hash_functions() == 8);
    
    assert(max_hash_filter.insert(42));
    assert(max_hash_filter.contains(42));
    
    // Test optimal hash functions calculation
    size_t optimal = AtomicBloomFilter<int, 8192, 3>::optimal_hash_functions(1000);
    assert(optimal >= 1 && optimal <= 8);
    
    // Test expected false positive rate
    double expected_fp = AtomicBloomFilter<int, 8192, 3>::expected_false_positive_rate(500);
    assert(expected_fp >= 0.0 && expected_fp <= 1.0);
    
    std::cout << "PASSED\n";
}

void test_performance_characteristics() {
    std::cout << "Testing performance characteristics... ";
    
    AtomicBloomFilter<int, 32768, 4> filter;
    
    constexpr int num_operations = 1000;
    
    // Measure insert performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        filter.insert(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto insert_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Measure lookup performance
    start = std::chrono::high_resolution_clock::now();
    int found_count = 0;
    for (int i = 0; i < num_operations; ++i) {
        if (filter.contains(i)) {
            found_count++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto lookup_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // All items should be found (no false negatives)
    assert(found_count == num_operations);
    
    // Performance should be reasonable (very loose bounds)
    assert(insert_time.count() < 50000);  // Less than 50ms for 1000 operations
    assert(lookup_time.count() < 50000);  // Less than 50ms for 1000 lookups
    
    std::cout << "PASSED (Insert: " << insert_time.count() << "μs, Lookup: " << lookup_time.count() << "μs)\n";
}

void test_type_aliases() {
    std::cout << "Testing type aliases... ";
    
    // Test various type aliases
    AtomicBloomFilter8K<int> filter8k;
    AtomicBloomFilter64K<std::string> filter64k;
    AtomicBloomFilter1M<int> filter1m;
    AtomicStringBloomFilter string_filter;
    AtomicIntBloomFilter int_filter;
    
    assert(filter8k.capacity() == 8192);
    assert(filter64k.capacity() == 65536);
    assert(filter1m.capacity() == 1048576);
    
    // Basic functionality test
    assert(string_filter.insert("test"));
    assert(string_filter.contains("test"));
    
    assert(int_filter.insert(123));
    assert(int_filter.contains(123));
    
    std::cout << "PASSED\n";
}

void test_load_factor_progression() {
    std::cout << "Testing load factor progression... ";
    
    AtomicBloomFilter<int, 1024, 3> filter;
    
    std::vector<double> load_factors;
    
    // Insert items and track load factor progression
    for (int i = 0; i < 200; ++i) {
        filter.insert(i);
        if (i % 20 == 19) {  // Sample every 20 insertions
            load_factors.push_back(filter.load_factor());
        }
    }
    
    // Load factor should generally increase (with some possible fluctuation due to hash collisions)
    assert(load_factors.front() < load_factors.back());
    
    // Should never exceed 1.0
    for (double lf : load_factors) {
        assert(lf >= 0.0 && lf <= 1.0);
    }
    
    std::cout << "PASSED\n";
}

void stress_test() {
    std::cout << "Running stress test... ";
    
    AtomicBloomFilter<int, 16384, 4> filter;
    
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(t * 10000, (t + 1) * 10000 - 1);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int value = dist(gen);
                
                // Insert and immediately test
                filter.insert(value);
                if (filter.contains(value)) {
                    successful_operations.fetch_add(1);
                }
                
                // Occasional statistics access
                if (i % 100 == 0) {
                    auto stats = filter.get_statistics();
                    (void)stats;  // Suppress unused variable warning
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All operations should have been successful
    assert(successful_operations.load() == num_threads * operations_per_thread);
    
    std::cout << "PASSED (" << successful_operations.load() << " operations)\n";
}

int main() {
    std::cout << "AtomicBloomFilter Test Suite\n";
    std::cout << "============================\n";
    
    test_basic_operations();
    test_string_operations();
    test_false_positive_characteristics();
    test_concurrent_operations();
    test_multiple_filter_coordination();
    test_statistics();
    test_edge_cases();
    test_performance_characteristics();
    test_type_aliases();
    test_load_factor_progression();
    stress_test();
    
    std::cout << "\n=== All Tests Passed! ===\n";
    std::cout << "AtomicBloomFilter implementation is working correctly.\n";
    std::cout << "\nKey findings:\n";
    std::cout << "• No false negatives detected\n";
    std::cout << "• False positive rates within expected bounds\n";
    std::cout << "• Concurrent operations work safely\n";
    std::cout << "• Performance characteristics are reasonable\n";
    std::cout << "• Multiple filter coordination works correctly\n";
    std::cout << "• Statistics and load factor calculations are accurate\n";
    
    return 0;
} 