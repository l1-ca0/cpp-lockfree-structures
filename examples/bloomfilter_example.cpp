#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <set>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include "lockfree/atomic_bloomfilter.hpp"

using namespace lockfree;

void test_basic_operations() {
    std::cout << "=== Basic Bloom Filter Operations ===\n";
    
    AtomicStringBloomFilter filter;
    
    std::cout << "Filter capacity: " << filter.capacity() << " bits\n";
    std::cout << "Hash functions: " << filter.num_hash_functions() << "\n";
    std::cout << "Initial empty: " << (filter.empty() ? "Yes" : "No") << "\n\n";
    
    // Insert some items
    std::vector<std::string> items = {
        "apple", "banana", "cherry", "date", "elderberry",
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };
    
    std::cout << "Inserting items:\n";
    for (const auto& item : items) {
        bool newly_inserted = filter.insert(item);
        std::cout << "  " << item << " -> " << (newly_inserted ? "New" : "Already present") << "\n";
    }
    
    std::cout << "\nChecking inserted items:\n";
    for (const auto& item : items) {
        bool might_contain = filter.contains(item);
        std::cout << "  " << item << " -> " << (might_contain ? "Might be present" : "Definitely not present") << "\n";
    }
    
    // Test some items that weren't inserted
    std::vector<std::string> test_items = {"orange", "pear", "mango", "coconut"};
    std::cout << "\nChecking non-inserted items:\n";
    for (const auto& item : test_items) {
        bool might_contain = filter.contains(item);
        std::cout << "  " << item << " -> " << (might_contain ? "False positive!" : "Correctly not found") << "\n";
    }
    
    // Display statistics
    auto stats = filter.get_statistics();
    std::cout << "\nFilter Statistics:\n";
    std::cout << "  Total bits: " << stats.total_bits << "\n";
    std::cout << "  Bits set: " << stats.bits_set << "\n";
    std::cout << "  Approximate items: " << stats.approximate_items << "\n";
    std::cout << "  Load factor: " << std::fixed << std::setprecision(4) << stats.load_factor << "\n";
    std::cout << "  False positive probability: " << std::fixed << std::setprecision(6) 
              << stats.false_positive_probability << "\n\n";
}

void test_false_positive_rate() {
    std::cout << "=== False Positive Rate Analysis ===\n";
    
    AtomicBloomFilter<int, 8192, 3> filter;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 100000);
    
    // Insert a known set of items
    std::set<int> inserted_items;
    constexpr int num_items = 500;
    
    std::cout << "Inserting " << num_items << " random integers...\n";
    for (int i = 0; i < num_items; ++i) {
        int value = dist(gen);
        inserted_items.insert(value);
        filter.insert(value);
    }
    
    std::cout << "Actually inserted " << inserted_items.size() << " unique items\n";
    
    // Test false positive rate
    constexpr int test_count = 10000;
    int false_positives = 0;
    int true_negatives = 0;
    
    std::cout << "Testing " << test_count << " random values for false positives...\n";
    
    for (int i = 0; i < test_count; ++i) {
        int test_value = dist(gen);
        bool in_filter = filter.contains(test_value);
        bool actually_inserted = inserted_items.count(test_value) > 0;
        
        if (in_filter && !actually_inserted) {
            false_positives++;
        } else if (!in_filter && !actually_inserted) {
            true_negatives++;
        }
    }
    
    double measured_fp_rate = static_cast<double>(false_positives) / (false_positives + true_negatives);
    double expected_fp_rate = filter.false_positive_probability();
    
    std::cout << "Results:\n";
    std::cout << "  False positives: " << false_positives << "\n";
    std::cout << "  True negatives: " << true_negatives << "\n";
    std::cout << "  Measured FP rate: " << std::fixed << std::setprecision(4) << measured_fp_rate << "\n";
    std::cout << "  Expected FP rate: " << std::fixed << std::setprecision(4) << expected_fp_rate << "\n";
    std::cout << "  Difference: " << std::fixed << std::setprecision(4) 
              << std::abs(measured_fp_rate - expected_fp_rate) << "\n\n";
}

void test_concurrent_access() {
    std::cout << "=== Concurrent Access Test ===\n";
    
    AtomicBloomFilter<std::string, 16384, 4> filter;
    
    constexpr int num_threads = 4;
    constexpr int items_per_thread = 200;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> thread_items(num_threads);
    std::atomic<int> total_insertions{0};
    std::atomic<int> successful_lookups{0};
    
    // Generate unique items for each thread
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < items_per_thread; ++i) {
            thread_items[t].push_back("thread_" + std::to_string(t) + "_item_" + std::to_string(i));
        }
    }
    
    std::cout << "Starting " << num_threads << " concurrent threads...\n";
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Launch insert threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            int local_insertions = 0;
            
            // Insert items
            for (const auto& item : thread_items[t]) {
                bool newly_inserted = filter.insert(item);
                if (newly_inserted) {
                    local_insertions++;
                }
                
                // Occasionally test lookups
                if (local_insertions % 50 == 0) {
                    bool found = filter.contains(item);
                    if (found) {
                        successful_lookups.fetch_add(1);
                    }
                }
            }
            
            total_insertions.fetch_add(local_insertions);
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Concurrent operations completed in " << duration.count() << " ms\n";
    std::cout << "Total insertions: " << total_insertions.load() << "\n";
    std::cout << "Successful concurrent lookups: " << successful_lookups.load() << "\n";
    
    // Verify all items can be found
    int verification_count = 0;
    for (int t = 0; t < num_threads; ++t) {
        for (const auto& item : thread_items[t]) {
            if (filter.contains(item)) {
                verification_count++;
            }
        }
    }
    
    std::cout << "Verification: " << verification_count << "/" 
              << (num_threads * items_per_thread) << " items found\n";
    
    auto final_stats = filter.get_statistics();
    std::cout << "Final load factor: " << std::fixed << std::setprecision(4) 
              << final_stats.load_factor << "\n\n";
}

void test_set_operations() {
    std::cout << "=== Set Operations (Union/Intersection) ===\n";
    
    AtomicBloomFilter<int, 4096, 3> filter1, filter2, filter3;
    
    // Populate first filter with even numbers
    std::cout << "Filter 1: Adding even numbers 2, 4, 6, ..., 20\n";
    for (int i = 2; i <= 20; i += 2) {
        filter1.insert(i);
    }
    
    // Populate second filter with multiples of 3
    std::cout << "Filter 2: Adding multiples of 3: 3, 6, 9, ..., 21\n";
    for (int i = 3; i <= 21; i += 3) {
        filter2.insert(i);
    }
    
    // Create union by manually inserting all elements
    std::cout << "Creating union filter by inserting all elements from both filters...\n";
    for (int i = 2; i <= 20; i += 2) {
        filter3.insert(i);
    }
    for (int i = 3; i <= 21; i += 3) {
        filter3.insert(i);
    }
    
    std::cout << "\nUnion filter contains:\n";
    for (int i = 1; i <= 25; ++i) {
        bool in_filter3 = filter3.contains(i);
        bool in_filter1 = filter1.contains(i);
        bool in_filter2 = filter2.contains(i);
        
        if (in_filter3) {
            std::cout << "  " << i << " (in filter1: " << (in_filter1 ? "yes" : "no")
                      << ", in filter2: " << (in_filter2 ? "yes" : "no") << ")\n";
        }
    }
    
    // Test intersection by inserting only common elements (multiples of 6)
    AtomicBloomFilter<int, 4096, 3> intersection_filter;
    std::cout << "\nCreating intersection filter with multiples of 6...\n";
    for (int i = 6; i <= 21; i += 6) {
        intersection_filter.insert(i);
    }
    
    std::cout << "\nIntersection filter contains:\n";
    for (int i = 1; i <= 25; ++i) {
        if (intersection_filter.contains(i)) {
            std::cout << "  " << i << " (should be multiples of 6)\n";
        }
    }
    
    std::cout << "\nStatistics comparison:\n";
    auto stats1 = filter1.get_statistics();
    auto stats2 = filter2.get_statistics();
    auto stats3 = filter3.get_statistics();
    auto stats_intersection = intersection_filter.get_statistics();
    
    std::cout << "  Filter 1     - Load: " << std::fixed << std::setprecision(4) << stats1.load_factor 
              << ", FP rate: " << stats1.false_positive_probability << "\n";
    std::cout << "  Filter 2     - Load: " << std::fixed << std::setprecision(4) << stats2.load_factor 
              << ", FP rate: " << stats2.false_positive_probability << "\n";
    std::cout << "  Union        - Load: " << std::fixed << std::setprecision(4) << stats3.load_factor 
              << ", FP rate: " << stats3.false_positive_probability << "\n";
    std::cout << "  Intersection - Load: " << std::fixed << std::setprecision(4) << stats_intersection.load_factor 
              << ", FP rate: " << stats_intersection.false_positive_probability << "\n\n";
}

void test_optimal_parameters() {
    std::cout << "=== Optimal Parameters Analysis ===\n";
    
    std::vector<size_t> expected_elements = {100, 500, 1000, 2000, 5000};
    
    std::cout << "Optimal hash functions for different expected element counts:\n";
    std::cout << "Expected Elements | Optimal Hash Functions | Expected FP Rate\n";
    std::cout << "------------------|------------------------|------------------\n";
    
    for (size_t expected : expected_elements) {
        size_t optimal_k = AtomicBloomFilter<int, 8192, 3>::optimal_hash_functions(expected);
        double expected_fp = AtomicBloomFilter<int, 8192, 3>::expected_false_positive_rate(expected);
        
        std::cout << std::setw(17) << expected << " | " 
                  << std::setw(22) << optimal_k << " | "
                  << std::fixed << std::setprecision(6) << std::setw(16) << expected_fp << "\n";
    }
    
    std::cout << "\nNote: This analysis uses a fixed filter size of 8192 bits.\n";
    std::cout << "For production use, consider adjusting both filter size and hash functions.\n\n";
}

void test_performance_comparison() {
    std::cout << "=== Performance Comparison ===\n";
    
    AtomicBloomFilter<int, 32768, 4> bloom_filter;
    std::set<int> std_set;
    
    constexpr int num_operations = 10000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 50000);
    
    // Generate test data
    std::vector<int> test_data;
    for (int i = 0; i < num_operations; ++i) {
        test_data.push_back(dist(gen));
    }
    
    // Benchmark Bloom filter insertions
    auto start = std::chrono::high_resolution_clock::now();
    for (int value : test_data) {
        bloom_filter.insert(value);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto bloom_insert_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Benchmark std::set insertions
    start = std::chrono::high_resolution_clock::now();
    for (int value : test_data) {
        std_set.insert(value);
    }
    end = std::chrono::high_resolution_clock::now();
    auto set_insert_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Benchmark Bloom filter lookups
    start = std::chrono::high_resolution_clock::now();
    int bloom_found = 0;
    for (int value : test_data) {
        if (bloom_filter.contains(value)) {
            bloom_found++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto bloom_lookup_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Benchmark std::set lookups
    start = std::chrono::high_resolution_clock::now();
    int set_found = 0;
    for (int value : test_data) {
        if (std_set.count(value) > 0) {
            set_found++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto set_lookup_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Performance comparison (" << num_operations << " operations):\n";
    std::cout << "\nInsertions:\n";
    std::cout << "  Bloom Filter: " << bloom_insert_time.count() << " μs\n";
    std::cout << "  std::set:     " << set_insert_time.count() << " μs\n";
    std::cout << "  Speedup:      " << std::fixed << std::setprecision(2) 
              << (static_cast<double>(set_insert_time.count()) / bloom_insert_time.count()) << "x\n";
    
    std::cout << "\nLookups:\n";
    std::cout << "  Bloom Filter: " << bloom_lookup_time.count() << " μs (found: " << bloom_found << ")\n";
    std::cout << "  std::set:     " << set_lookup_time.count() << " μs (found: " << set_found << ")\n";
    std::cout << "  Speedup:      " << std::fixed << std::setprecision(2) 
              << (static_cast<double>(set_lookup_time.count()) / bloom_lookup_time.count()) << "x\n";
    
    std::cout << "\nMemory usage:\n";
    std::cout << "  Bloom Filter: " << (bloom_filter.capacity() / 8) << " bytes\n";
    std::cout << "  std::set:     ~" << (std_set.size() * sizeof(int) * 3) << " bytes (estimated)\n";
    
    auto stats = bloom_filter.get_statistics();
    std::cout << "  Filter load:  " << std::fixed << std::setprecision(2) 
              << (stats.load_factor * 100) << "%\n\n";
}

int main() {
    std::cout << "Lock-free Bloom Filter Example\n";
    std::cout << "===============================\n\n";
    
    test_basic_operations();
    test_false_positive_rate();
    test_concurrent_access();
    test_set_operations();
    test_optimal_parameters();
    test_performance_comparison();
    
    std::cout << "=== Summary ===\n\n";
    std::cout << "Key characteristics of Bloom filters:\n";
    std::cout << "• Space-efficient probabilistic data structure\n";
    std::cout << "• No false negatives (if not found, definitely not in set)\n";
    std::cout << "• Possible false positives (if found, might be in set)\n";
    std::cout << "• Insert and query operations are O(k) where k is number of hash functions\n";
    std::cout << "• Cannot remove elements (use Counting Bloom Filter for that)\n";
    std::cout << "• Perfect for applications like caching, databases, and network protocols\n";
    std::cout << "• This lock-free implementation supports safe concurrent access\n\n";
    
    std::cout << "Bloom filter example completed successfully!\n";
    return 0;
} 