#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include <set>
#include "lockfree/atomic_linkedlist.hpp"

using namespace lockfree;

void test_basic_linkedlist_operations() {
    std::cout << "=== Basic LinkedList Operations ===\n";
    
    AtomicLinkedList<int> list;
    
    std::cout << "Initial state - empty: " << list.empty() 
              << ", size: " << list.size() << "\n";
    
    // Insert elements
    std::cout << "Inserting elements...\n";
    assert(list.insert(5));
    assert(list.insert(2));
    assert(list.insert(8));
    assert(list.insert(1));
    assert(list.insert(9));
    
    std::cout << "After insertions - size: " << list.size() << "\n";
    
    // Test duplicate insertion
    std::cout << "Testing duplicate insertion:\n";
    bool duplicate_result = list.insert(5);
    std::cout << "  Inserting duplicate '5': " 
              << (duplicate_result ? "Success" : "Failed (expected)") << "\n";
    
    // Test lookups
    std::cout << "\nTesting lookups:\n";
    std::vector<int> test_values = {1, 3, 5, 8, 10};
    
    for (int val : test_values) {
        bool found = list.contains(val);
        std::cout << "  " << val << " -> " << (found ? "found" : "not found") << "\n";
    }
    
    std::cout << "Final state - size: " << list.size() << "\n\n";
}

void test_concurrent_linkedlist_operations() {
    std::cout << "=== Concurrent LinkedList Operations ===\n";
    
    AtomicLinkedList<int> list;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 250;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_removes{0};
    std::atomic<int> successful_finds{0};
    std::atomic<int> failed_finds{0};
    std::atomic<bool> start_flag{false};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/remove/find threads
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
                
                if (op < 50) {  // 50% insert operations
                    if (list.insert(value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 70) {  // 20% remove operations
                    if (list.remove(value)) {
                        successful_removes.fetch_add(1);
                    }
                } else {  // 30% find operations
                    if (list.find(value)) {
                        successful_finds.fetch_add(1);
                    } else {
                        failed_finds.fetch_add(1);
                    }
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
    std::cout << "Successful removes: " << successful_removes.load() << "\n";
    std::cout << "Successful finds: " << successful_finds.load() << "\n";
    std::cout << "Failed finds: " << failed_finds.load() << "\n";
    std::cout << "Final list size: " << list.size() << "\n\n";
}

void test_string_linkedlist() {
    std::cout << "=== String LinkedList Operations ===\n";
    
    AtomicLinkedList<std::string> list;
    
    // Insert words
    std::vector<std::string> words = {
        "apple", "banana", "cherry", "date", "elderberry", 
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };
    
    std::cout << "Inserting words...\n";
    for (const auto& word : words) {
        list.insert(word);
    }
    
    std::cout << "List size: " << list.size() << "\n";
    
    // Test lookups
    std::cout << "\nWord lookups:\n";
    std::vector<std::string> lookup_words = {
        "apple", "banana", "mango", "cherry", "orange"
    };
    
    for (const auto& word : lookup_words) {
        bool found = list.contains(word);
        std::cout << "  " << word << " -> " << (found ? "found" : "not found") << "\n";
    }
    
    // Remove some words
    std::cout << "\nRemoving some words...\n";
    list.remove("banana");
    list.remove("elderberry");
    list.remove("kiwi");
    
    std::cout << "After removals - size: " << list.size() << "\n\n";
}

void test_producer_consumer_pattern() {
    std::cout << "=== Producer-Consumer Pattern ===\n";
    
    AtomicLinkedList<int> shared_list;
    constexpr int num_producers = 2;
    constexpr int num_consumers = 2;
    constexpr int items_per_producer = 100;
    
    std::atomic<bool> production_done{false};
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    
    std::vector<std::thread> threads;
    
    // Producer threads
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int item = p * items_per_producer + i;
                if (shared_list.insert(item)) {
                    items_produced.fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // Consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        threads.emplace_back([&, c]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, num_producers * items_per_producer - 1);
            
            while (!production_done.load() || !shared_list.empty()) {
                int target = dist(gen);
                if (shared_list.remove(target)) {
                    items_consumed.fetch_add(1);
                    if (items_consumed.load() % 25 == 0) {
                        std::cout << "Consumer " << c << " removed item " << target << "\n";
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
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
    std::cout << "  Items produced: " << items_produced.load() << "\n";
    std::cout << "  Items consumed: " << items_consumed.load() << "\n";
    std::cout << "  Remaining in list: " << shared_list.size() << "\n\n";
}

void test_iteration() {
    std::cout << "=== LinkedList Iteration ===\n";
    
    AtomicLinkedList<int> list;
    
    // Insert some numbers
    std::vector<int> numbers = {5, 2, 8, 1, 9, 3, 7, 4, 6};
    for (int num : numbers) {
        list.insert(num);
    }
    
    std::cout << "Inserted " << list.size() << " numbers\n";
    std::cout << "Iterating through list:\n";
    
    int count = 0;
    for (auto it = list.begin(); it != list.end(); ++it) {
        std::cout << "  " << *it << "\n";
        ++count;
        if (count >= 15) break; // Safety limit
    }
    
    std::cout << "Iterated through " << count << " elements\n\n";
}

void test_emplace_operations() {
    std::cout << "=== Emplace Operations ===\n";
    
    AtomicLinkedList<std::pair<int, std::string>> list;
    
    // Test emplace with pair construction
    list.emplace(1, "first");
    list.emplace(2, "second");
    list.emplace(3, "third");
    
    std::cout << "Emplaced 3 pairs\n";
    std::cout << "List size: " << list.size() << "\n";
    
    // Test lookups
    std::pair<int, std::string> target1{1, "first"};
    std::pair<int, std::string> target2{2, "second"};
    std::pair<int, std::string> target3{4, "fourth"};
    
    std::cout << "Looking for pairs:\n";
    std::cout << "  (1, \"first\") -> " << (list.contains(target1) ? "found" : "not found") << "\n";
    std::cout << "  (2, \"second\") -> " << (list.contains(target2) ? "found" : "not found") << "\n";
    std::cout << "  (4, \"fourth\") -> " << (list.contains(target3) ? "found" : "not found") << "\n\n";
}

void test_concurrent_stress() {
    std::cout << "=== Concurrent Stress Test ===\n";
    
    AtomicLinkedList<int> list;
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 500;
    constexpr int value_range = 100;
    
    std::atomic<int> total_operations{0};
    std::atomic<bool> start_flag{false};
    
    std::vector<std::thread> threads;
    
    // Stress test with heavy contention
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> value_dist(1, value_range);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int value = value_dist(gen);
                int op = op_dist(gen);
                
                if (op < 40) {
                    list.insert(value);
                } else if (op < 70) {
                    list.remove(value);
                } else {
                    list.find(value);
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
    std::cout << "Final list size: " << list.size() << "\n";
    std::cout << "Operations per second: " << (total_operations.load() * 1000) / duration.count() << "\n\n";
}

int main() {
    std::cout << "Lock-free LinkedList Example\n";
    std::cout << "=============================\n\n";
    
    test_basic_linkedlist_operations();
    test_concurrent_linkedlist_operations();
    test_string_linkedlist();
    test_producer_consumer_pattern();
    test_iteration();
    test_emplace_operations();
    test_concurrent_stress();
    
    std::cout << "All LinkedList tests completed!\n";
    std::cout << "\nNote: This LinkedList implementation provides lock-free\n";
    std::cout << "operations with logical deletion and hazard pointer protection.\n";
    std::cout << "The list maintains insertion order and provides strong consistency.\n";
    
    return 0;
}