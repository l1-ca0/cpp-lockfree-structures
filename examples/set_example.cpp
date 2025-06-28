#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include <algorithm>
#include "lockfree/atomic_set.hpp"

using namespace lockfree;

void test_basic_set_operations() {
    std::cout << "=== Basic Set Operations ===\n";
    
    AtomicSet<int> set;
    
    std::cout << "Initial state - empty: " << set.empty() 
              << ", size: " << set.size() << "\n";
    
    // Insert elements
    std::cout << "Inserting elements...\n";
    assert(set.insert(5));
    assert(set.insert(2));
    assert(set.insert(8));
    assert(set.insert(1));
    assert(set.insert(9));
    
    std::cout << "After insertions - size: " << set.size() << "\n";
    
    // Test duplicate insertion
    std::cout << "Testing duplicate insertion:\n";
    bool duplicate_result = set.insert(5);
    std::cout << "  Inserting duplicate '5': " 
              << (duplicate_result ? "Success" : "Failed (expected)") << "\n";
    
    // Test membership
    std::cout << "\nTesting membership:\n";
    std::vector<int> test_values = {1, 3, 5, 8, 10};
    
    for (int val : test_values) {
        bool found = set.contains(val);
        std::cout << "  " << val << " -> " << (found ? "in set" : "not in set") << "\n";
    }
    
    std::cout << "Final state - size: " << set.size() << "\n\n";
}

void test_set_operations() {
    std::cout << "=== Set Operations ===\n";
    
    AtomicSet<int> set1;
    AtomicSet<int> set2;
    
    // Populate set1
    for (int i = 1; i <= 10; ++i) {
        set1.insert(i);
    }
    
    // Populate set2 (overlapping with set1)
    for (int i = 6; i <= 15; ++i) {
        set2.insert(i);
    }
    
    std::cout << "Set1 size: " << set1.size() << "\n";
    std::cout << "Set2 size: " << set2.size() << "\n";
    
    // Test subset relationships
    AtomicSet<int> subset;
    subset.insert(2);
    subset.insert(4);
    subset.insert(6);
    
    std::cout << "Subset {2,4,6} is subset of set1: " 
              << subset.is_subset_of(set1) << "\n";
    std::cout << "Set1 is superset of subset: " 
              << set1.is_superset_of(subset) << "\n";
    
    // Count elements matching a condition
    auto even_count = set1.count_if([](int x) { return x % 2 == 0; });
    std::cout << "Even numbers in set1: " << even_count << "\n\n";
}

void test_concurrent_set_operations() {
    std::cout << "=== Concurrent Set Operations ===\n";
    
    AtomicSet<int> shared_set;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 250;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_erases{0};
    std::atomic<int> membership_tests{0};
    std::atomic<bool> start_flag{false};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/erase/contains threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> value_dist(1, 200);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int value = value_dist(gen);
                int op = op_dist(gen);
                
                if (op < 60) {  // 60% insert operations
                    if (shared_set.insert(value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 80) {  // 20% erase operations
                    if (shared_set.erase(value)) {
                        successful_erases.fetch_add(1);
                    }
                } else {  // 20% membership tests
                    shared_set.contains(value);
                    membership_tests.fetch_add(1);
                }
                
                // Small delay to increase contention
                if (i % 50 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
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
    std::cout << "Successful erases: " << successful_erases.load() << "\n";
    std::cout << "Membership tests: " << membership_tests.load() << "\n";
    std::cout << "Final set size: " << shared_set.size() << "\n\n";
}

void test_string_set() {
    std::cout << "=== String Set Operations ===\n";
    
    AtomicSet<std::string> word_set;
    
    // Insert words
    std::vector<std::string> words = {
        "apple", "banana", "cherry", "date", "elderberry", 
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };
    
    std::cout << "Inserting words...\n";
    for (const auto& word : words) {
        word_set.insert(word);
    }
    
    std::cout << "Set size: " << word_set.size() << "\n";
    
    // Test membership
    std::cout << "\nWord membership tests:\n";
    std::vector<std::string> test_words = {
        "apple", "banana", "mango", "cherry", "orange"
    };
    
    for (const auto& word : test_words) {
        bool found = word_set.contains(word);
        std::cout << "  " << word << " -> " << (found ? "in set" : "not in set") << "\n";
    }
    
    // Remove some words
    std::cout << "\nRemoving some words...\n";
    word_set.erase("banana");
    word_set.erase("elderberry");
    word_set.erase("kiwi");
    
    std::cout << "After removals - size: " << word_set.size() << "\n\n";
}

void test_unique_elements_filter() {
    std::cout << "=== Unique Elements Filter ===\n";
    
    AtomicSet<int> unique_numbers;
    
    // Simulate data stream with duplicates
    std::vector<int> data_stream = {
        1, 3, 2, 1, 4, 2, 5, 3, 6, 1, 7, 4, 8, 2, 9, 5
    };
    
    std::cout << "Processing data stream with duplicates...\n";
    std::cout << "Input: ";
    for (int num : data_stream) {
        std::cout << num << " ";
    }
    std::cout << "\n";
    
    // Filter unique elements
    for (int num : data_stream) {
        if (unique_numbers.insert(num)) {
            std::cout << "New unique number: " << num << "\n";
        }
    }
    
    std::cout << "Total unique numbers: " << unique_numbers.size() << "\n";
    
    // Convert to sorted vector
    auto unique_vector = unique_numbers.to_vector();
    std::sort(unique_vector.begin(), unique_vector.end());
    
    std::cout << "Unique numbers (sorted): ";
    for (int num : unique_vector) {
        std::cout << num << " ";
    }
    std::cout << "\n\n";
}

void test_emplace_operations() {
    std::cout << "=== Emplace Operations ===\n";
    
    AtomicSet<std::pair<int, std::string>> pair_set;
    
    // Test emplace with pair construction
    pair_set.emplace(1, "first");
    pair_set.emplace(2, "second");
    pair_set.emplace(3, "third");
    
    std::cout << "Emplaced 3 pairs\n";
    std::cout << "Set size: " << pair_set.size() << "\n";
    
    // Test membership
    std::pair<int, std::string> target1{1, "first"};
    std::pair<int, std::string> target2{2, "second"};
    std::pair<int, std::string> target3{4, "fourth"};
    
    std::cout << "Membership tests:\n";
    std::cout << "  (1, \"first\") -> " << (pair_set.contains(target1) ? "in set" : "not in set") << "\n";
    std::cout << "  (2, \"second\") -> " << (pair_set.contains(target2) ? "in set" : "not in set") << "\n";
    std::cout << "  (4, \"fourth\") -> " << (pair_set.contains(target3) ? "in set" : "not in set") << "\n\n";
}

void test_iteration() {
    std::cout << "=== Set Iteration ===\n";
    
    AtomicSet<char> char_set;
    
    // Insert some characters
    for (char c = 'a'; c <= 'j'; ++c) {
        char_set.insert(c);
    }
    
    std::cout << "Inserted " << char_set.size() << " characters\n";
    std::cout << "Iterating through set:\n";
    
    int count = 0;
    for (auto it = char_set.begin(); it != char_set.end(); ++it) {
        std::cout << "  " << *it << "\n";
        ++count;
        if (count >= 15) break; // Safety limit
    }
    
    std::cout << "Iterated through " << count << " elements\n\n";
}

void test_producer_consumer_pattern() {
    std::cout << "=== Producer-Consumer Pattern ===\n";
    
    AtomicSet<int> processed_ids;
    constexpr int num_producers = 2;
    constexpr int num_consumers = 2;
    constexpr int ids_per_producer = 50;
    
    std::atomic<bool> production_done{false};
    std::atomic<int> total_produced{0};
    std::atomic<int> duplicates_filtered{0};
    
    std::vector<std::thread> threads;
    
    // Producer threads (generate work IDs)
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> id_dist(1, 30); // Intentional overlap
            
            for (int i = 0; i < ids_per_producer; ++i) {
                int work_id = id_dist(gen);
                total_produced.fetch_add(1);
                
                if (!processed_ids.insert(work_id)) {
                    duplicates_filtered.fetch_add(1);
                    std::cout << "Producer " << p << " filtered duplicate ID: " << work_id << "\n";
                } else {
                    std::cout << "Producer " << p << " added new ID: " << work_id << "\n";
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }
    
    // Consumer threads (process unique IDs)
    for (int c = 0; c < num_consumers; ++c) {
        threads.emplace_back([&, c]() {
            while (!production_done.load() || !processed_ids.empty()) {
                // Try to find and process an ID
                for (auto it = processed_ids.begin(); it != processed_ids.end(); ++it) {
                    int id = *it;
                    if (processed_ids.erase(id)) {
                        std::cout << "Consumer " << c << " processed ID: " << id << "\n";
                        break;
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }
    
    // Wait for producers to finish
    for (int i = 0; i < num_producers; ++i) {
        threads[i].join();
    }
    production_done.store(true);
    
    // Wait for consumers to finish
    for (int i = num_producers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    std::cout << "Producer-consumer results:\n";
    std::cout << "  Total IDs generated: " << total_produced.load() << "\n";
    std::cout << "  Duplicates filtered: " << duplicates_filtered.load() << "\n";
    std::cout << "  Unique IDs processed: " << (total_produced.load() - duplicates_filtered.load()) << "\n";
    std::cout << "  Remaining in set: " << processed_ids.size() << "\n\n";
}

int main() {
    std::cout << "Lock-free Set Example\n";
    std::cout << "=====================\n\n";
    
    test_basic_set_operations();
    test_set_operations();
    test_concurrent_set_operations();
    test_string_set();
    test_unique_elements_filter();
    test_emplace_operations();
    test_iteration();
    test_producer_consumer_pattern();
    
    std::cout << "All Set tests completed!\n";
    std::cout << "\nNote: This Set implementation provides lock-free operations\n";
    std::cout << "with hash-table based storage and hazard pointer protection.\n";
    std::cout << "It's ideal for concurrent membership testing and deduplication.\n";
    
    return 0;
}