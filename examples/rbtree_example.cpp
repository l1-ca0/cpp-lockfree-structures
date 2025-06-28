#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include <set>
#include "lockfree/atomic_rbtree.hpp"

using namespace lockfree;

void test_basic_tree_operations() {
    std::cout << "=== Basic Red-Black Tree Operations ===\n";
    
    AtomicRBTree<int, std::string> tree;
    
    // Insert key-value pairs
    std::cout << "Inserting key-value pairs...\n";
    tree.insert(5, "Five");
    tree.insert(2, "Two");
    tree.insert(8, "Eight");
    tree.insert(1, "One");
    tree.insert(3, "Three");
    tree.insert(7, "Seven");
    tree.insert(9, "Nine");
    tree.insert(4, "Four");
    tree.insert(6, "Six");
    
    std::cout << "Tree size: " << tree.size() << "\n";
    
    // Test lookups
    std::cout << "\nTesting lookups:\n";
    std::string value;
    for (int key : {1, 3, 5, 7, 9, 10}) {
        if (tree.find(key, value)) {
            std::cout << "  Key " << key << " -> " << value << "\n";
        } else {
            std::cout << "  Key " << key << " not found\n";
        }
    }
    
    // Test duplicate insertion
    std::cout << "\nTesting duplicate insertion:\n";
    bool inserted = tree.insert(5, "Five Again");
    std::cout << "  Inserting duplicate key 5: " << (inserted ? "Success" : "Failed (expected)") << "\n";
    
    std::cout << "Final tree size: " << tree.size() << "\n\n";
}

void test_concurrent_dictionary() {
    std::cout << "=== Concurrent Dictionary Operations ===\n";
    
    AtomicRBTree<std::string, int> dictionary;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 250;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_lookups{0};
    std::atomic<int> failed_lookups{0};
    
    std::vector<std::string> words = {
        "apple", "banana", "cherry", "date", "elderberry", "fig", "grape",
        "honeydew", "kiwi", "lemon", "mango", "nectarine", "orange", "papaya",
        "quince", "raspberry", "strawberry", "tangerine", "watermelon"
    };
    
    std::vector<std::thread> threads;
    
    // Mixed insert/lookup threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> word_dist(0, words.size() - 1);
            std::uniform_int_distribution<> op_dist(0, 99);
            std::uniform_int_distribution<> value_dist(1, 1000);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                std::string word = words[word_dist(gen)];
                
                if (op_dist(gen) < 60) {
                    // 60% insert operations
                    int value = t * operations_per_thread + i;
                    if (dictionary.insert(word, value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else {
                    // 40% lookup operations
                    int found_value;
                    if (dictionary.find(word, found_value)) {
                        successful_lookups.fetch_add(1);
                    } else {
                        failed_lookups.fetch_add(1);
                    }
                }
                
                // Small delay to increase contention
                if (i % 50 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Concurrent operations completed in " << duration.count() << " ms\n";
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Successful lookups: " << successful_lookups.load() << "\n";
    std::cout << "Failed lookups: " << failed_lookups.load() << "\n";
    std::cout << "Final tree size: " << dictionary.size() << "\n";
    
    // Verify all words are accessible
    std::cout << "\nVerifying dictionary contents:\n";
    for (const auto& word : words) {
        int value;
        if (dictionary.find(word, value)) {
            std::cout << "  " << word << " -> " << value << "\n";
        }
    }
    std::cout << "\n";
}

void test_range_operations() {
    std::cout << "=== Range Operations and Iteration ===\n";
    
    AtomicRBTree<int, std::string> tree;
    
    // Insert numbers with string representations
    std::vector<int> numbers = {15, 10, 20, 8, 12, 25, 6, 11, 13, 22, 27};
    
    for (int num : numbers) {
        std::string str_val = "Value_" + std::to_string(num);
        tree.insert(num, str_val);
    }
    
    std::cout << "Inserted " << numbers.size() << " elements\n";
    std::cout << "Tree size: " << tree.size() << "\n";
    
    // Test ordered iteration
    std::cout << "\nIn-order traversal:\n";
    int count = 0;
    for (auto it = tree.begin(); it != tree.end() && count < 20; ++it, ++count) {
        auto [key, value] = *it;
        std::cout << "  " << key << " -> " << value << "\n";
    }
    
    std::cout << "Traversed " << count << " elements\n\n";
}

void test_concurrent_updates() {
    std::cout << "=== Concurrent Updates Test ===\n";
    
    AtomicRBTree<int, int> counter_tree;
    constexpr int num_counters = 50;
    constexpr int num_threads = 6;
    constexpr int increments_per_thread = 100;
    
    // Initialize counters
    for (int i = 0; i < num_counters; ++i) {
        counter_tree.emplace(i, 0);
    }
    
    std::cout << "Initialized " << num_counters << " counters\n";
    
    std::atomic<int> total_increments{0};
    
    std::vector<std::thread> threads;
    
    // Concurrent increment threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> counter_dist(0, num_counters - 1);
            
            for (int i = 0; i < increments_per_thread; ++i) {
                int counter_id = counter_dist(gen);
                
                // This is a simplified approach - in practice, you'd need
                // more sophisticated concurrent update mechanisms
                int current_value;
                if (counter_tree.find(counter_id, current_value)) {
                    // Note: This is not truly atomic for the tree update
                    // A real implementation would need compare-and-swap loops
                    counter_tree.insert(counter_id, current_value + 1);
                    total_increments.fetch_add(1);
                }
                
                if (i % 25 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }
        });
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Concurrent updates completed in " << duration.count() << " ms\n";
    std::cout << "Total increments attempted: " << total_increments.load() << "\n";
    
    // Check final counter values
    std::cout << "\nFinal counter values (first 10):\n";
    for (int i = 0; i < std::min(10, num_counters); ++i) {
        int value;
        if (counter_tree.find(i, value)) {
            std::cout << "  Counter " << i << ": " << value << "\n";
        }
    }
    
    std::cout << "Tree size: " << counter_tree.size() << "\n\n";
}

int main() {
    std::cout << "Lock-free Red-Black Tree Example\n";
    std::cout << "=================================\n\n";
    
    test_basic_tree_operations();
    test_concurrent_dictionary();
    test_range_operations();
    test_concurrent_updates();
    
    std::cout << "All red-black tree tests completed!\n";
    std::cout << "\nNote: The red-black tree implementation shown is simplified\n";
    std::cout << "for demonstration. A production lock-free red-black tree would\n";
    std::cout << "require more sophisticated algorithms for rotations and balancing.\n";
    
    return 0;
}