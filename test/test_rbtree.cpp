#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <map>
#include <string>
#include <random>
#include <algorithm>
#include "lockfree/atomic_rbtree.hpp"

using namespace lockfree;

void test_basic_rbtree_operations() {
    std::cout << "Testing basic red-black tree operations...\n";
    
    AtomicRBTree<int, std::string> tree;
    
    assert(tree.empty());
    assert(tree.size() == 0);
    
    // Test insert
    assert(tree.insert(5, "five"));
    assert(tree.insert(3, "three"));
    assert(tree.insert(7, "seven"));
    assert(tree.insert(1, "one"));
    assert(tree.insert(9, "nine"));
    
    assert(!tree.empty());
    assert(tree.size() == 5);
    
    // Test duplicate insert
    assert(!tree.insert(5, "five_duplicate"));
    assert(tree.size() == 5);
    
    // Test find
    std::string value;
    assert(tree.find(5, value));
    assert(value == "five");
    
    assert(tree.find(1, value));
    assert(value == "one");
    
    assert(tree.find(9, value));
    assert(value == "nine");
    
    // Test find non-existent
    assert(!tree.find(10, value));
    assert(!tree.find(0, value));
    
    std::cout << "Basic red-black tree operations test passed!\n";
}

void test_integer_tree() {
    std::cout << "Testing integer key-value tree...\n";
    
    AtomicRBTree<int, int> tree;
    
    std::vector<int> keys = {15, 10, 20, 8, 12, 25, 6, 11, 13, 22, 27};
    
    // Insert key-value pairs
    for (int key : keys) {
        assert(tree.insert(key, key * 10));
    }
    
    assert(tree.size() == keys.size());
    
    // Verify all insertions
    for (int key : keys) {
        int value;
        assert(tree.find(key, value));
        assert(value == key * 10);
    }
    
    // Test range of non-existent keys
    for (int key = 1; key <= 5; ++key) {
        int value;
        assert(!tree.find(key, value));
    }
    
    std::cout << "Integer tree test passed!\n";
}

void test_string_keys() {
    std::cout << "Testing string keys...\n";
    
    AtomicRBTree<std::string, int> tree;
    
    std::vector<std::pair<std::string, int>> data = {
        {"apple", 1}, {"banana", 2}, {"cherry", 3}, {"date", 4}, {"elderberry", 5}
    };
    
    // Insert data
    for (const auto& pair : data) {
        assert(tree.insert(pair.first, pair.second));
    }
    
    // Verify lookups
    for (const auto& pair : data) {
        int value;
        assert(tree.find(pair.first, value));
        assert(value == pair.second);
    }
    
    // Test non-existent keys
    int value;
    assert(!tree.find("grape", value));
    assert(!tree.find("fig", value));
    
    std::cout << "String keys test passed!\n";
}

void test_concurrent_rbtree_operations() {
    std::cout << "Testing concurrent red-black tree operations...\n";
    
    AtomicRBTree<int, int> tree;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 200;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_lookups{0};
    std::atomic<int> failed_lookups{0};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/lookup threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 1000);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                
                if (op_dist(gen) < 70) {  // 70% insert operations
                    if (tree.insert(key, key * 100)) {
                        successful_inserts.fetch_add(1);
                    }
                } else {  // 30% lookup operations
                    int value;
                    if (tree.find(key, value)) {
                        successful_lookups.fetch_add(1);
                        assert(value == key * 100);
                    } else {
                        failed_lookups.fetch_add(1);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Successful lookups: " << successful_lookups.load() << "\n";
    std::cout << "Failed lookups: " << failed_lookups.load() << "\n";
    std::cout << "Final tree size: " << tree.size() << "\n";
    
    // Basic consistency check
    assert(tree.size() <= successful_inserts.load());
    
    std::cout << "Concurrent red-black tree operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicRBTree<int, std::pair<int, std::string>> tree;
    
    assert(tree.emplace(1, 10, "ten"));
    assert(tree.emplace(2, 20, "twenty"));
    assert(tree.emplace(3, 30, "thirty"));
    
    std::pair<int, std::string> value;
    assert(tree.find(2, value));
    assert(value.first == 20 && value.second == "twenty");
    
    // Test duplicate emplace
    assert(!tree.emplace(2, 200, "two_hundred"));
    
    std::cout << "Emplace operations test passed!\n";
}

void test_iteration() {
    std::cout << "Testing tree iteration...\n";
    
    AtomicRBTree<int, std::string> tree;
    
    std::vector<int> keys = {5, 2, 8, 1, 3, 7, 9};
    std::vector<std::string> values = {"five", "two", "eight", "one", "three", "seven", "nine"};
    
    // Insert in random order
    for (size_t i = 0; i < keys.size(); ++i) {
        tree.insert(keys[i], values[i]);
    }
    
    // Iterate and collect keys
    std::vector<int> iterated_keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        auto [key, value] = *it;
        iterated_keys.push_back(key);
    }
    
    // Should be in sorted order
    std::vector<int> expected_keys = keys;
    std::sort(expected_keys.begin(), expected_keys.end());
    
    assert(iterated_keys.size() == expected_keys.size());
    for (size_t i = 0; i < iterated_keys.size(); ++i) {
        assert(iterated_keys[i] == expected_keys[i]);
    }
    
    std::cout << "Tree iteration test passed!\n";
}

void test_tree_stress() {
    std::cout << "Testing tree stress test...\n";
    
    AtomicRBTree<int, int> tree;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 500;
    
    std::atomic<int> insert_attempts{0};
    std::atomic<int> successful_inserts{0};
    std::atomic<int> lookup_attempts{0};
    std::atomic<int> successful_lookups{0};
    
    std::vector<std::thread> threads;
    
    // Stress test with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 500);  // Overlapping keys
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                
                if (op_dist(gen) < 50) {  // 50% insert operations
                    insert_attempts.fetch_add(1);
                    if (tree.insert(key, key * 2)) {
                        successful_inserts.fetch_add(1);
                    }
                } else {  // 50% lookup operations
                    lookup_attempts.fetch_add(1);
                    int value;
                    if (tree.find(key, value)) {
                        successful_lookups.fetch_add(1);
                        assert(value == key * 2);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Insert attempts: " << insert_attempts.load() << "\n";
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Lookup attempts: " << lookup_attempts.load() << "\n";
    std::cout << "Successful lookups: " << successful_lookups.load() << "\n";
    std::cout << "Final tree size: " << tree.size() << "\n";
    
    // Verify tree integrity
    assert(tree.size() == successful_inserts.load());
    assert(tree.size() <= insert_attempts.load());
    
    std::cout << "Tree stress test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicRBTree<int, std::unique_ptr<int>> tree;
    
    assert(tree.insert(1, std::make_unique<int>(100)));
    assert(tree.insert(2, std::make_unique<int>(200)));
    
    std::unique_ptr<int> value;
    assert(tree.find(1, value));
    assert(*value == 100);
    
    assert(tree.find(2, value));
    assert(*value == 200);
    
    std::cout << "Move semantics test passed!\n";
}

int main() {
    std::cout << "AtomicRBTree Tests\n";
    std::cout << "==================\n\n";
    
    test_basic_rbtree_operations();
    test_integer_tree();
    test_string_keys();
    test_concurrent_rbtree_operations();
    test_emplace_operations();
    test_iteration();
    test_tree_stress();
    test_move_semantics();
    
    std::cout << "\nAll red-black tree tests passed!\n";
    std::cout << "\nNote: This implementation provides a simplified lock-free tree\n";
    std::cout << "for demonstration purposes. A production implementation would\n";
    std::cout << "require more sophisticated balancing and rotation algorithms.\n";
    
    return 0;
}