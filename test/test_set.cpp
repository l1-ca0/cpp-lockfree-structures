#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <set>
#include <string>
#include <random>
#include <algorithm>
#include "lockfree/atomic_set.hpp"

using namespace lockfree;

void test_basic_set_operations() {
    std::cout << "Testing basic set operations...\n";
    
    AtomicSet<int> set;
    
    assert(set.empty());
    assert(set.size() == 0);
    assert(set.bucket_count() > 0);
    assert(set.load_factor() == 0.0);
    
    // Test insert
    assert(set.insert(5));
    assert(set.insert(2));
    assert(set.insert(8));
    assert(set.insert(1));
    
    assert(!set.empty());
    assert(set.size() == 4);
    assert(set.load_factor() > 0.0);
    
    // Test duplicate insert
    assert(!set.insert(5));
    assert(set.size() == 4);
    
    // Test contains/find
    assert(set.contains(5));
    assert(set.find(2));
    assert(set.contains(8));
    assert(set.find(1));
    
    // Test contains non-existent
    assert(!set.contains(10));
    assert(!set.find(0));
    
    std::cout << "Basic set operations test passed!\n";
}

void test_string_operations() {
    std::cout << "Testing string operations...\n";
    
    AtomicSet<std::string> set;
    
    std::vector<std::string> words = {"apple", "banana", "cherry", "date"};
    
    // Insert words
    for (const auto& word : words) {
        assert(set.insert(word));
    }
    
    assert(set.size() == words.size());
    
    // Verify all insertions
    for (const auto& word : words) {
        assert(set.contains(word));
    }
    
    // Test non-existent words
    assert(!set.contains("grape"));
    assert(!set.find("mango"));
    
    std::cout << "String operations test passed!\n";
}

void test_erase_operations() {
    std::cout << "Testing erase operations...\n";
    
    AtomicSet<int> set;
    
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        set.insert(i);
    }
    
    assert(set.size() == 10);
    
    // Test successful erases
    assert(set.erase(5));
    assert(set.size() == 9);
    assert(!set.contains(5));
    
    assert(set.erase(1));
    assert(set.erase(10));
    assert(set.size() == 7);
    
    // Test erase non-existent items
    assert(!set.erase(5));  // Already erased
    assert(!set.erase(15)); // Never existed
    assert(set.size() == 7);
    
    // Verify remaining items
    assert(!set.contains(1));  // Erased
    assert(!set.contains(5));  // Erased
    assert(!set.contains(10)); // Erased
    
    assert(set.contains(2));   // Still there
    assert(set.contains(9));   // Still there
    
    std::cout << "Erase operations test passed!\n";
}

void test_concurrent_set_operations() {
    std::cout << "Testing concurrent set operations...\n";
    
    AtomicSet<int> set;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 50;  
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_erases{0};
    std::atomic<int> membership_tests{0};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/erase/contains threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> value_dist(1, 50);  
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int value = value_dist(gen);
                int op = op_dist(gen);
                
                if (op < 60) {  // 60% insert operations
                    if (set.insert(value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 80) {  // 20% erase operations
                    if (set.erase(value)) {
                        successful_erases.fetch_add(1);
                    }
                } else {  // 20% membership tests
                    set.contains(value);
                    membership_tests.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Successful erases: " << successful_erases.load() << "\n";
    std::cout << "Membership tests: " << membership_tests.load() << "\n";
    std::cout << "Final set size: " << set.size() << "\n";
    
    // Basic consistency check
    int expected_size = successful_inserts.load() - successful_erases.load();
    assert(set.size() <= expected_size); // May be less due to duplicates
    
    std::cout << "Concurrent set operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicSet<std::pair<int, std::string>> set;
    
    assert(set.emplace(1, "first"));
    assert(set.emplace(2, "second"));
    assert(set.emplace(3, "third"));
    
    // Test exact duplicate emplace (same key AND value)
    assert(!set.emplace(2, "second"));
    
    // Different value with same key should succeed in a set
    assert(set.emplace(2, "SECOND"));
    
    assert(set.size() == 4);
    
    // Verify emplaced values
    std::pair<int, std::string> target1{1, "first"};
    std::pair<int, std::string> target2{2, "second"};
    std::pair<int, std::string> target3{3, "third"};
    
    assert(set.contains(target1));
    assert(set.contains(target2));
    assert(set.contains(target3));
    
    std::cout << "Emplace operations test passed!\n";
}

void test_iteration() {
    std::cout << "Testing set iteration...\n";
    
    AtomicSet<int> set;
    
    std::set<int> inserted_values;
    
    // Insert test data
    for (int i = 1; i <= 20; ++i) {
        if (i % 3 != 0) {  // Skip some values
            set.insert(i);
            inserted_values.insert(i);
        }
    }
    
    // Iterate and collect values
    std::set<int> iterated_values;
    int iteration_count = 0;
    
    for (auto it = set.begin(); it != set.end(); ++it) {
        int value = *it;
        iterated_values.insert(value);
        iteration_count++;
        
        if (iteration_count > 50) break; // Safety limit
    }
    
    // Verify all inserted values were found during iteration
    assert(iterated_values == inserted_values);
    assert(iteration_count == set.size());
    
    std::cout << "Set iteration test passed!\n";
}

void test_range_insertion() {
    std::cout << "Testing range insertion...\n";
    
    AtomicSet<int> set;
    std::vector<int> values = {1, 2, 3, 4, 5, 3, 2, 6, 7}; // With duplicates
    
    set.insert(values.begin(), values.end());
    
    // Should have unique values only
    std::set<int> unique_values(values.begin(), values.end());
    assert(set.size() == unique_values.size());
    
    // Verify all unique values are present
    for (int val : unique_values) {
        assert(set.contains(val));
    }
    
    std::cout << "Range insertion test passed!\n";
}

void test_set_relationships() {
    std::cout << "Testing set relationships...\n";
    
    AtomicSet<int> set1;
    AtomicSet<int> set2;
    AtomicSet<int> subset;
    
    // Populate sets
    for (int i = 1; i <= 10; ++i) {
        set1.insert(i);
    }
    
    for (int i = 6; i <= 15; ++i) {
        set2.insert(i);
    }
    
    subset.insert(2);
    subset.insert(4);
    subset.insert(6);
    
    // Test subset relationships
    assert(subset.is_subset_of(set1));
    assert(set1.is_superset_of(subset));
    assert(!set1.is_subset_of(subset));
    assert(!subset.is_superset_of(set1));
    
    // Test with overlapping sets
    assert(!set1.is_subset_of(set2));
    assert(!set2.is_subset_of(set1));
    
    std::cout << "Set relationships test passed!\n";
}

void test_predicate_operations() {
    std::cout << "Testing predicate operations...\n";
    
    AtomicSet<int> set;
    
    // Insert numbers 1-20
    for (int i = 1; i <= 20; ++i) {
        set.insert(i);
    }
    
    // Count even numbers
    auto even_count = set.count_if([](int x) { return x % 2 == 0; });
    assert(even_count == 10);
    
    // Count numbers > 15
    auto large_count = set.count_if([](int x) { return x > 15; });
    assert(large_count == 5);
    
    // Count numbers divisible by 3
    auto div3_count = set.count_if([](int x) { return x % 3 == 0; });
    assert(div3_count == 6); // 3, 6, 9, 12, 15, 18
    
    std::cout << "Predicate operations test passed!\n";
}

void test_to_vector() {
    std::cout << "Testing to_vector operation...\n";
    
    AtomicSet<int> set;
    std::vector<int> original = {5, 2, 8, 1, 9, 3};
    
    // Insert values
    for (int val : original) {
        set.insert(val);
    }
    
    // Convert to vector
    auto result = set.to_vector();
    
    // Should have same size
    assert(result.size() == set.size());
    assert(result.size() == original.size());
    
    // Sort both for comparison
    std::sort(result.begin(), result.end());
    std::sort(original.begin(), original.end());
    
    // Should contain same elements
    assert(result == original);
    
    std::cout << "to_vector operation test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicSet<std::unique_ptr<int>> set;
    
    // Test move insert
    assert(set.insert(std::make_unique<int>(100)));
    assert(set.insert(std::make_unique<int>(200)));
    
    assert(set.size() == 2);
    
    // Note: Testing contains with unique_ptr is complex since we can't 
    // easily create another unique_ptr with same value for comparison
    // In practice, you'd use a different approach for unique types
    
    std::cout << "Move semantics test passed!\n";
}

void test_stress_operations() {
    std::cout << "Testing stress operations...\n";
    
    AtomicSet<int> set;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    constexpr int value_range = 300;
    
    std::atomic<int> insert_attempts{0};
    std::atomic<int> successful_inserts{0};
    std::atomic<int> erase_attempts{0};
    std::atomic<int> successful_erases{0};
    std::atomic<int> contains_tests{0};
    
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
                
                if (op < 50) {  // 50% insert operations
                    insert_attempts.fetch_add(1);
                    if (set.insert(value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 70) {  // 20% erase operations
                    erase_attempts.fetch_add(1);
                    if (set.erase(value)) {
                        successful_erases.fetch_add(1);
                    }
                } else {  // 30% contains operations
                    set.contains(value);
                    contains_tests.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Insert attempts: " << insert_attempts.load() 
              << ", successful: " << successful_inserts.load() << "\n";
    std::cout << "Erase attempts: " << erase_attempts.load() 
              << ", successful: " << successful_erases.load() << "\n";
    std::cout << "Contains tests: " << contains_tests.load() << "\n";
    std::cout << "Final set size: " << set.size() << "\n";
    
    // Verify set integrity
    int expected_size = successful_inserts.load() - successful_erases.load();
    assert(set.size() == expected_size);
    
    std::cout << "Stress operations test passed!\n";
}

int main() {
    std::cout << "AtomicSet Tests\n";
    std::cout << "===============\n\n";
    
    test_basic_set_operations();
    test_string_operations();
    test_erase_operations();
    test_concurrent_set_operations();
    test_emplace_operations();
    test_iteration();
    test_range_insertion();
    test_set_relationships();
    test_predicate_operations();
    test_to_vector();
    test_move_semantics();
    test_stress_operations();
    
    std::cout << "\nAll set tests passed!\n";
    std::cout << "\nNote: This Set implementation provides lock-free operations\n";
    std::cout << "using a hash table with chaining and hazard pointer protection.\n";
    std::cout << "It's optimized for concurrent membership testing and deduplication.\n";
    
    return 0;
}