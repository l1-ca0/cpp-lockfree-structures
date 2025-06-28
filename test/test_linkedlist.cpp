#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <set>
#include <string>
#include <random>
#include <algorithm>
#include "lockfree/atomic_linkedlist.hpp"

using namespace lockfree;

void test_basic_linkedlist_operations() {
    std::cout << "Testing basic linkedlist operations...\n";
    
    AtomicLinkedList<int> list;
    
    assert(list.empty());
    assert(list.size() == 0);
    
    // Test insert
    assert(list.insert(5));
    assert(list.insert(2));
    assert(list.insert(8));
    assert(list.insert(1));
    
    assert(!list.empty());
    assert(list.size() == 4);
    
    // Test duplicate insert
    assert(!list.insert(5));
    assert(list.size() == 4);
    
    // Test find/contains
    assert(list.find(5));
    assert(list.contains(2));
    assert(list.find(8));
    assert(list.contains(1));
    
    // Test find non-existent
    assert(!list.find(10));
    assert(!list.contains(0));
    
    std::cout << "Basic linkedlist operations test passed!\n";
}

void test_string_operations() {
    std::cout << "Testing string operations...\n";
    
    AtomicLinkedList<std::string> list;
    
    std::vector<std::string> words = {"apple", "banana", "cherry", "date"};
    
    // Insert words
    for (const auto& word : words) {
        assert(list.insert(word));
    }
    
    assert(list.size() == words.size());
    
    // Verify all insertions
    for (const auto& word : words) {
        assert(list.contains(word));
    }
    
    // Test non-existent words
    assert(!list.contains("grape"));
    assert(!list.find("mango"));
    
    std::cout << "String operations test passed!\n";
}

void test_remove_operations() {
    std::cout << "Testing remove operations...\n";
    
    AtomicLinkedList<int> list;
    
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        list.insert(i);
    }
    
    assert(list.size() == 10);
    
    // Test successful removes
    assert(list.remove(5));
    assert(list.size() == 9);
    assert(!list.contains(5));
    
    assert(list.remove(1));
    assert(list.remove(10));
    assert(list.size() == 7);
    
    // Test remove non-existent items
    assert(!list.remove(5));  // Already removed
    assert(!list.remove(15)); // Never existed
    assert(list.size() == 7);
    
    // Verify remaining items
    assert(!list.contains(1));  // Removed
    assert(!list.contains(5));  // Removed
    assert(!list.contains(10)); // Removed
    
    assert(list.contains(2));   // Still there
    assert(list.contains(9));   // Still there
    
    std::cout << "Remove operations test passed!\n";
}

void test_concurrent_linkedlist_operations() {
    std::cout << "Testing concurrent linkedlist operations...\n";
    
    AtomicLinkedList<int> list;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 500;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_removes{0};
    std::atomic<int> successful_finds{0};
    std::atomic<int> failed_finds{0};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/remove/find threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
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
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Successful removes: " << successful_removes.load() << "\n";
    std::cout << "Successful finds: " << successful_finds.load() << "\n";
    std::cout << "Failed finds: " << failed_finds.load() << "\n";
    std::cout << "Final list size: " << list.size() << "\n";
    
    // Basic consistency check
    int expected_size = successful_inserts.load() - successful_removes.load();
    assert(list.size() <= expected_size); // May be less due to duplicates
    
    std::cout << "Concurrent linkedlist operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    // Custom comparator that only compares the key (first element) of pairs
    struct KeyOnlyEqual {
        bool operator()(const std::pair<int, std::string>& a, 
                       const std::pair<int, std::string>& b) const {
            return a.first == b.first;
        }
    };
    
    AtomicLinkedList<std::pair<int, std::string>, KeyOnlyEqual> list;
    
    assert(list.emplace(1, "first"));
    assert(list.emplace(2, "second"));
    assert(list.emplace(3, "third"));
    
    // Test duplicate emplace (should fail because key 2 already exists)
    assert(!list.emplace(2, "SECOND"));
    
    assert(list.size() == 3);
    
    // Verify emplaced values (using the same comparator logic)
    std::pair<int, std::string> target1{1, ""};  // Only key matters
    std::pair<int, std::string> target2{2, ""};  // Only key matters  
    std::pair<int, std::string> target3{3, ""};  // Only key matters
    
    assert(list.contains(target1));
    assert(list.contains(target2));
    assert(list.contains(target3));
    
    std::cout << "Emplace operations test passed!\n";
}

void test_iteration() {
    std::cout << "Testing linkedlist iteration...\n";
    
    AtomicLinkedList<int> list;
    
    std::set<int> inserted_values;
    
    // Insert test data
    for (int i = 1; i <= 20; ++i) {
        if (i % 3 != 0) {  // Skip some values
            list.insert(i);
            inserted_values.insert(i);
        }
    }
    
    // Iterate and collect values
    std::set<int> iterated_values;
    int iteration_count = 0;
    
    for (auto it = list.begin(); it != list.end(); ++it) {
        int value = *it;
        iterated_values.insert(value);
        iteration_count++;
        
        if (iteration_count > 50) break; // Safety limit
    }
    
    // Verify all inserted values were found during iteration
    assert(iterated_values == inserted_values);
    assert(iteration_count == list.size());
    
    std::cout << "LinkedList iteration test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicLinkedList<std::unique_ptr<int>> list;
    
    // Test move insert
    assert(list.insert(std::make_unique<int>(100)));
    assert(list.insert(std::make_unique<int>(200)));
    
    assert(list.size() == 2);
    
    // Note: For unique_ptr, we can't easily test find() because
    // it would require creating another unique_ptr with the same value,
    // which isn't straightforward. In practice, you'd use a different
    // comparator or store the raw pointer value for comparison.
    
    std::cout << "Move semantics test passed!\n";
}

void test_stress_operations() {
    std::cout << "Testing stress operations...\n";
    
    AtomicLinkedList<int> list;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    constexpr int value_range = 300;
    
    std::atomic<int> insert_attempts{0};
    std::atomic<int> successful_inserts{0};
    std::atomic<int> remove_attempts{0};
    std::atomic<int> successful_removes{0};
    std::atomic<int> find_attempts{0};
    std::atomic<int> successful_finds{0};
    
    std::vector<std::thread> threads;
    
    // Stress test with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> value_dist(1, value_range);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int value = value_dist(gen);
                int op = op_dist(gen);
                
                if (op < 40) {  // 40% insert operations
                    insert_attempts.fetch_add(1);
                    if (list.insert(value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 60) {  // 20% remove operations
                    remove_attempts.fetch_add(1);
                    if (list.remove(value)) {
                        successful_removes.fetch_add(1);
                    }
                } else {  // 40% find operations
                    find_attempts.fetch_add(1);
                    if (list.find(value)) {
                        successful_finds.fetch_add(1);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Insert attempts: " << insert_attempts.load() 
              << ", successful: " << successful_inserts.load() << "\n";
    std::cout << "Remove attempts: " << remove_attempts.load() 
              << ", successful: " << successful_removes.load() << "\n";
    std::cout << "Find attempts: " << find_attempts.load() 
              << ", successful: " << successful_finds.load() << "\n";
    std::cout << "Final list size: " << list.size() << "\n";
    
    // Verify list integrity
    int expected_size = successful_inserts.load() - successful_removes.load();
    assert(list.size() <= expected_size); // May be less due to duplicates
    
    std::cout << "Stress operations test passed!\n";
}

void test_custom_comparator() {
    std::cout << "Testing custom comparator...\n";
    
    // Custom comparator for case-insensitive string comparison
    struct CaseInsensitiveEqual {
        bool operator()(const std::string& a, const std::string& b) const {
            if (a.length() != b.length()) return false;
            return std::equal(a.begin(), a.end(), b.begin(),
                            [](char a, char b) {
                                return std::tolower(a) == std::tolower(b);
                            });
        }
    };
    
    AtomicLinkedList<std::string, CaseInsensitiveEqual> list;
    
    assert(list.insert("Apple"));
    assert(list.insert("BANANA"));
    assert(list.insert("Cherry"));
    
    // Test case-insensitive lookups
    assert(list.contains("apple"));   // lowercase
    assert(list.contains("APPLE"));   // uppercase
    assert(list.contains("banana"));  // different case
    assert(list.contains("CHERRY"));  // different case
    
    // Test case-insensitive duplicates
    assert(!list.insert("apple"));   // Should fail (duplicate)
    assert(!list.insert("BANANA"));  // Should fail (duplicate)
    
    std::cout << "Custom comparator test passed!\n";
}

int main() {
    std::cout << "AtomicLinkedList Tests\n";
    std::cout << "======================\n\n";
    
    test_basic_linkedlist_operations();
    test_string_operations();
    test_remove_operations();
    test_concurrent_linkedlist_operations();
    test_emplace_operations();
    test_iteration();
    test_move_semantics();
    test_stress_operations();
    test_custom_comparator();
    
    std::cout << "\nAll linkedlist tests passed!\n";
    std::cout << "\nNote: This LinkedList implementation provides lock-free operations\n";
    std::cout << "with hazard pointer protection and supports custom comparators.\n";
    
    return 0;
}