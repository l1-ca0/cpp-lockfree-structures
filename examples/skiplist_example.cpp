#include <iostream>
#include <iomanip>  
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include <map>
#include "lockfree/atomic_skiplist.hpp"

using namespace lockfree;

void test_basic_skiplist_operations() {
    std::cout << "=== Basic SkipList Operations ===\n";
    
    AtomicSkipList<int, std::string> skiplist;
    
    std::cout << "Initial state - empty: " << skiplist.empty() 
              << ", size: " << skiplist.size() << "\n";
    
    // Insert key-value pairs
    std::cout << "Inserting key-value pairs...\n";
    assert(skiplist.insert(5, "five"));
    assert(skiplist.insert(2, "two"));
    assert(skiplist.insert(8, "eight"));
    assert(skiplist.insert(1, "one"));
    assert(skiplist.insert(9, "nine"));
    assert(skiplist.insert(3, "three"));
    assert(skiplist.insert(7, "seven"));
    
    std::cout << "After insertions - size: " << skiplist.size() << "\n";
    
    // Test duplicate insertion
    std::cout << "Testing duplicate insertion:\n";
    bool duplicate_result = skiplist.insert(5, "FIVE");
    std::cout << "  Inserting duplicate key '5': " 
              << (duplicate_result ? "Success" : "Failed (expected)") << "\n";
    
    // Test lookups
    std::cout << "\nTesting lookups:\n";
    std::vector<int> test_keys = {1, 3, 5, 8, 10};
    
    for (int key : test_keys) {
        std::string value;
        if (skiplist.find(key, value)) {
            std::cout << "  " << key << " -> " << value << "\n";
        } else {
            std::cout << "  " << key << " -> not found\n";
        }
    }
    
    std::cout << "Final state - size: " << skiplist.size() << "\n\n";
}

void test_ordered_operations() {
    std::cout << "=== Ordered Operations ===\n";
    
    AtomicSkipList<int, int> skiplist;
    
    // Insert numbers in random order
    std::vector<int> numbers = {15, 3, 8, 1, 12, 7, 20, 5, 18, 10};
    
    std::cout << "Inserting numbers in random order...\n";
    for (int num : numbers) {
        skiplist.insert(num, num * 10);
    }
    
    std::cout << "SkipList size: " << skiplist.size() << "\n";
    
    // Iterate in sorted order
    std::cout << "\nIterating in sorted order:\n";
    int count = 0;
    for (auto it = skiplist.begin(); it != skiplist.end(); ++it) {
        auto [key, value] = *it;
        std::cout << "  " << key << " -> " << value << "\n";
        ++count;
        if (count >= 15) break; // Safety limit
    }
    
    std::cout << "Iterated through " << count << " elements\n\n";
}

void test_concurrent_skiplist_operations() {
    std::cout << "=== Concurrent SkipList Operations ===\n";
    
    AtomicSkipList<int, std::string> skiplist;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 200;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_finds{0};
    std::atomic<int> failed_finds{0};
    std::atomic<int> successful_erases{0};
    std::atomic<bool> start_flag{false};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/find/erase threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 150);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op < 60) {  // 60% insert operations
                    std::string value = "Thread" + std::to_string(t) + "_Item" + std::to_string(i);
                    if (skiplist.insert(key, value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 85) {  // 25% find operations
                    std::string found_value;
                    if (skiplist.find(key, found_value)) {
                        successful_finds.fetch_add(1);
                    } else {
                        failed_finds.fetch_add(1);
                    }
                } else {  // 15% erase operations
                    if (skiplist.erase(key)) {
                        successful_erases.fetch_add(1);
                    }
                }
                
                // Small delay to increase contention
                if (i % 50 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }
        });
    }
    
    std::cout << "Starting concurrent operations...\n";
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Start all threads
    start_flag.store(true);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Concurrent operations completed in " << duration.count() << " ms\n";
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Successful finds: " << successful_finds.load() << "\n";
    std::cout << "Failed finds: " << failed_finds.load() << "\n";
    std::cout << "Successful erases: " << successful_erases.load() << "\n";
    std::cout << "Final skiplist size: " << skiplist.size() << "\n\n";
}

void test_string_key_skiplist() {
    std::cout << "=== String Key SkipList ===\n";
    
    AtomicSkipList<std::string, int> dictionary;
    
    // Create a word frequency counter
    std::vector<std::pair<std::string, int>> word_freq = {
        {"apple", 15}, {"banana", 8}, {"cherry", 22}, {"date", 5},
        {"elderberry", 3}, {"fig", 12}, {"grape", 18}, {"honey", 25},
        {"kiwi", 7}, {"lemon", 14}, {"mango", 20}, {"orange", 30}
    };
    
    std::cout << "Building word frequency dictionary...\n";
    for (const auto& [word, freq] : word_freq) {
        dictionary.insert(word, freq);
    }
    
    std::cout << "Dictionary size: " << dictionary.size() << "\n";
    
    // Test lookups
    std::cout << "\nWord frequency lookups:\n";
    std::vector<std::string> lookup_words = {
        "apple", "banana", "mango", "pineapple", "cherry"
    };
    
    for (const auto& word : lookup_words) {
        int frequency;
        if (dictionary.find(word, frequency)) {
            std::cout << "  " << word << " -> frequency: " << frequency << "\n";
        } else {
            std::cout << "  " << word << " -> not found\n";
        }
    }
    
    // Show sorted order
    std::cout << "\nWords in alphabetical order:\n";
    int count = 0;
    for (auto it = dictionary.begin(); it != dictionary.end(); ++it) {
        auto [word, freq] = *it;
        std::cout << "  " << word << " (" << freq << ")\n";
        if (++count >= 10) break;
    }
    std::cout << "\n";
}

void test_range_queries_simulation() {
    std::cout << "=== Range Queries Simulation ===\n";
    
    AtomicSkipList<int, std::string> data;
    
    // Insert test data
    for (int i = 10; i <= 100; i += 5) {
        std::string value = "Value_" + std::to_string(i);
        data.insert(i, value);
    }
    
    std::cout << "Inserted " << data.size() << " data points\n";
    
    // Simulate range queries
    auto range_query = [&](int start, int end) {
        std::cout << "Range [" << start << ", " << end << "]:\n";
        int count = 0;
        
        for (auto it = data.begin(); it != data.end(); ++it) {
            auto [key, value] = *it;
            if (key >= start && key <= end) {
                std::cout << "  " << key << " -> " << value << "\n";
                count++;
            } else if (key > end) {
                break; // Skip list is ordered, no need to continue
            }
        }
        
        std::cout << "Found " << count << " items in range\n\n";
    };
    
    range_query(25, 45);
    range_query(60, 80);
    range_query(5, 15);
}

void test_emplace_operations() {
    std::cout << "=== Emplace Operations ===\n";
    
    AtomicSkipList<int, std::pair<std::string, double>> skiplist;
    
    // Test emplace with pair construction
    skiplist.emplace(1, "first", 1.1);
    skiplist.emplace(2, "second", 2.2);
    skiplist.emplace(3, "third", 3.3);
    
    std::cout << "Emplaced 3 entries\n";
    std::cout << "SkipList size: " << skiplist.size() << "\n";
    
    // Test lookups
    std::pair<std::string, double> value;
    for (int key = 1; key <= 3; ++key) {
        if (skiplist.find(key, value)) {
            std::cout << "  Key " << key << " -> (" 
                      << value.first << ", " << value.second << ")\n";
        }
    }
    std::cout << "\n";
}

void test_performance_comparison() {
    std::cout << "=== Performance Comparison ===\n";
    
    AtomicSkipList<int, int> skiplist;
    std::map<int, int> std_map;
    
    constexpr int num_operations = 10000;
    
    // SkipList performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        skiplist.insert(i, i * 2);
    }
    
    int found_count = 0;
    int value;
    for (int i = 0; i < num_operations; i += 2) {
        if (skiplist.find(i, value)) {
            found_count++;
        }
    }
    
    auto skiplist_time = std::chrono::high_resolution_clock::now() - start_time;
    auto skiplist_ms = std::chrono::duration_cast<std::chrono::milliseconds>(skiplist_time);
    
    // std::map performance (single-threaded)
    start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        std_map[i] = i * 2;
    }
    
    int std_found_count = 0;
    for (int i = 0; i < num_operations; i += 2) {
        if (std_map.find(i) != std_map.end()) {
            std_found_count++;
        }
    }
    
    auto std_map_time = std::chrono::high_resolution_clock::now() - start_time;
    auto std_map_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std_map_time);
    
    std::cout << "Single-threaded performance comparison:\n";
    std::cout << "  SkipList: " << skiplist_ms.count() << " ms (found: " << found_count << ")\n";
    std::cout << "  std::map: " << std_map_ms.count() << " ms (found: " << std_found_count << ")\n";
    std::cout << "  SkipList size: " << skiplist.size() << "\n";
    std::cout << "  std::map size: " << std_map.size() << "\n\n";
}

void test_concurrent_stress() {
    std::cout << "=== Concurrent Stress Test ===\n";
    
    AtomicSkipList<int, int> skiplist;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    constexpr int key_range = 500;
    
    std::atomic<int> total_operations{0};
    std::atomic<bool> start_flag{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, key_range);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op < 50) {
                    skiplist.insert(key, key * 100);
                } else if (op < 80) {
                    int value;
                    skiplist.find(key, value);
                } else {
                    skiplist.erase(key);
                }
                
                total_operations.fetch_add(1);
            }
        });
    }
    
    std::cout << "Running stress test with " << num_threads << " threads...\n";
    auto start_time = std::chrono::high_resolution_clock::now();
    
    start_flag.store(true);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Stress test completed in " << duration.count() << " ms\n";
    std::cout << "Total operations: " << total_operations.load() << "\n";
    std::cout << "Final skiplist size: " << skiplist.size() << "\n";
    std::cout << "Operations per second: " << (total_operations.load() * 1000) / duration.count() << "\n\n";
}

int main() {
    std::cout << "Lock-free SkipList Example\n";
    std::cout << "===========================\n\n";
    
    test_basic_skiplist_operations();
    test_ordered_operations();
    test_concurrent_skiplist_operations();
    test_string_key_skiplist();
    test_range_queries_simulation();
    test_emplace_operations();
    test_performance_comparison();
    test_concurrent_stress();
    
    std::cout << "All SkipList tests completed!\n";
    std::cout << "\nNote: This SkipList implementation provides lock-free operations\n";
    std::cout << "with probabilistic balancing. It maintains sorted order and provides\n";
    std::cout << "O(log n) expected performance for search, insertion, and deletion.\n";
    
    return 0;
}