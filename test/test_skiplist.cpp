#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <set>
#include <string>
#include <random>
#include <algorithm>
#include <map>
#include "lockfree/atomic_skiplist.hpp"

using namespace lockfree;

void test_basic_skiplist_operations() {
    std::cout << "Testing basic skiplist operations...\n";
    
    AtomicSkipList<int, std::string> skiplist;
    
    assert(skiplist.empty());
    assert(skiplist.size() == 0);
    
    // Test insert
    assert(skiplist.insert(5, "five"));
    assert(skiplist.insert(2, "two"));
    assert(skiplist.insert(8, "eight"));
    assert(skiplist.insert(1, "one"));
    
    assert(!skiplist.empty());
    assert(skiplist.size() == 4);
    
    // Test duplicate insert
    assert(!skiplist.insert(5, "FIVE"));
    assert(skiplist.size() == 4);
    
    // Test find
    std::string value;
    assert(skiplist.find(5, value));
    assert(value == "five");
    
    assert(skiplist.find(2, value));
    assert(value == "two");
    
    assert(skiplist.find(8, value));
    assert(value == "eight");
    
    // Test find non-existent
    assert(!skiplist.find(10, value));
    assert(!skiplist.find(0, value));
    
    std::cout << "Basic skiplist operations test passed!\n";
}

void test_ordered_insertion() {
    std::cout << "Testing ordered insertion and iteration...\n";
    
    AtomicSkipList<int, int> skiplist;
    
    std::vector<int> numbers = {15, 3, 8, 1, 12, 7, 20, 5, 18, 10};
    
    // Insert in random order
    for (int num : numbers) {
        assert(skiplist.insert(num, num * 10));
    }
    
    assert(skiplist.size() == numbers.size());
    
    // Verify all numbers can be found
    for (int num : numbers) {
        int value;
        assert(skiplist.find(num, value));
        assert(value == num * 10);
    }
    
    // Test iteration order (should be sorted)
    std::vector<int> iterated_keys;
    for (auto it = skiplist.begin(); it != skiplist.end(); ++it) {
        auto [key, value] = *it;
        iterated_keys.push_back(key);
        assert(value == key * 10);
    }
    
    // Verify sorted order
    std::vector<int> expected_keys = numbers;
    std::sort(expected_keys.begin(), expected_keys.end());
    
    assert(iterated_keys == expected_keys);
    
    std::cout << "Ordered insertion test passed!\n";
}

void test_erase_operations() {
    std::cout << "Testing erase operations...\n";
    
    AtomicSkipList<int, std::string> skiplist;
    
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        skiplist.insert(i, "value" + std::to_string(i));
    }
    
    assert(skiplist.size() == 10);
    
    // Test successful erases
    assert(skiplist.erase(5));
    assert(skiplist.size() == 9);
    
    assert(skiplist.erase(1));
    assert(skiplist.erase(10));
    assert(skiplist.size() == 7);
    
    // Test erase non-existent keys
    assert(!skiplist.erase(5));  // Already erased
    assert(!skiplist.erase(15)); // Never existed
    assert(skiplist.size() == 7);
    
    // Verify remaining keys
    std::string value;
    assert(!skiplist.find(1, value));  // Erased
    assert(!skiplist.find(5, value));  // Erased
    assert(!skiplist.find(10, value)); // Erased
    
    assert(skiplist.find(2, value));   // Still there
    assert(skiplist.find(9, value));   // Still there
    
    std::cout << "Erase operations test passed!\n";
}

void test_string_keys() {
    std::cout << "Testing string key operations...\n";
    
    AtomicSkipList<std::string, int> skiplist;
    
    std::map<std::string, int> test_data = {
        {"apple", 1}, {"banana", 2}, {"cherry", 3}, 
        {"date", 4}, {"elderberry", 5}
    };
    
    // Insert data
    for (const auto& [key, val] : test_data) {
        assert(skiplist.insert(key, val));
    }
    
    assert(skiplist.size() == test_data.size());
    
    // Verify all insertions
    for (const auto& [key, expected_val] : test_data) {
        int found_val;
        assert(skiplist.find(key, found_val));
        assert(found_val == expected_val);
    }
    
    // Test iteration order (should be alphabetical)
    std::vector<std::string> iterated_keys;
    for (auto it = skiplist.begin(); it != skiplist.end(); ++it) {
        auto [key, value] = *it;
        iterated_keys.push_back(key);
    }
    
    // Verify alphabetical order
    std::vector<std::string> expected_keys;
    for (const auto& [key, val] : test_data) {
        expected_keys.push_back(key);
    }
    std::sort(expected_keys.begin(), expected_keys.end());
    
    assert(iterated_keys == expected_keys);
    
    std::cout << "String key operations test passed!\n";
}

void test_concurrent_skiplist_operations() {
    std::cout << "Testing concurrent skiplist operations...\n";
    
    AtomicSkipList<int, int> skiplist;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 500;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_finds{0};
    std::atomic<int> failed_finds{0};
    std::atomic<int> successful_erases{0};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/find/erase threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 200);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op < 60) {  // 60% insert operations
                    int value = t * operations_per_thread + i;
                    if (skiplist.insert(key, value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 85) {  // 25% find operations
                    int found_value;
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
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Successful finds: " << successful_finds.load() << "\n";
    std::cout << "Failed finds: " << failed_finds.load() << "\n";
    std::cout << "Successful erases: " << successful_erases.load() << "\n";
    std::cout << "Final skiplist size: " << skiplist.size() << "\n";
    
    // Basic consistency check
    int expected_size = successful_inserts.load() - successful_erases.load();
    assert(skiplist.size() == expected_size);
    
    std::cout << "Concurrent skiplist operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicSkipList<int, std::pair<std::string, double>> skiplist;
    
    assert(skiplist.emplace(1, "first", 1.1));
    assert(skiplist.emplace(2, "second", 2.2));
    assert(skiplist.emplace(3, "third", 3.3));
    
    // Test duplicate emplace
    assert(!skiplist.emplace(2, "SECOND", 999.9));
    
    assert(skiplist.size() == 3);
    
    // Verify emplaced values
    std::pair<std::string, double> value;
    assert(skiplist.find(1, value));
    assert(value.first == "first" && std::abs(value.second - 1.1) < 0.001);
    
    assert(skiplist.find(2, value));
    assert(value.first == "second" && std::abs(value.second - 2.2) < 0.001);
    
    std::cout << "Emplace operations test passed!\n";
}

void test_iteration() {
    std::cout << "Testing skiplist iteration...\n";
    
    AtomicSkipList<int, std::string> skiplist;
    
    std::map<int, std::string> inserted_data;
    
    // Insert test data
    for (int i = 1; i <= 20; ++i) {
        if (i % 3 != 0) {  // Skip some values
            std::string value = "value" + std::to_string(i);
            skiplist.insert(i, value);
            inserted_data[i] = value;
        }
    }
    
    // Iterate and collect data
    std::map<int, std::string> iterated_data;
    int iteration_count = 0;
    
    for (auto it = skiplist.begin(); it != skiplist.end(); ++it) {
        auto [key, value] = *it;
        iterated_data[key] = value;
        iteration_count++;
        
        if (iteration_count > 50) break; // Safety limit
    }
    
    // Verify all inserted data was found during iteration
    assert(iterated_data == inserted_data);
    assert(iteration_count == skiplist.size());
    
    std::cout << "SkipList iteration test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicSkipList<int, std::unique_ptr<int>> skiplist;
    
    // Test move insert
    assert(skiplist.insert(1, std::make_unique<int>(100)));
    assert(skiplist.insert(2, std::make_unique<int>(200)));
    
    assert(skiplist.size() == 2);
    
    // Test contains with move semantics
    assert(skiplist.contains(1));
    assert(skiplist.contains(2));
    assert(!skiplist.contains(3));
    
    // Test find_if with move semantics
    bool found1 = skiplist.find_if(1, [](const std::unique_ptr<int>& ptr) {
        assert(*ptr == 100);
        return true;  // Return true to indicate the condition was met
    });
    assert(found1);
    
    bool found2 = skiplist.find_if(2, [](const std::unique_ptr<int>& ptr) {
        assert(*ptr == 200);
        return true;  // Return true to indicate the condition was met
    });
    assert(found2);
    
    // Test that non-existent key returns false
    bool found3 = skiplist.find_if(3, [](const std::unique_ptr<int>& ptr) {
        assert(false); // Should not be called
        return false;  // This should never be reached
    });
    assert(!found3);
    
    std::cout << "Move semantics test passed!\n";
}

void test_stress_operations() {
    std::cout << "Testing stress operations...\n";
    
    AtomicSkipList<int, int> skiplist;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    constexpr int key_range = 300;
    
    std::atomic<int> insert_attempts{0};
    std::atomic<int> successful_inserts{0};
    std::atomic<int> erase_attempts{0};
    std::atomic<int> successful_erases{0};
    std::atomic<int> find_attempts{0};
    std::atomic<int> successful_finds{0};
    
    std::vector<std::thread> threads;
    
    // Stress test with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, key_range);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op < 50) {  // 50% insert operations
                    insert_attempts.fetch_add(1);
                    if (skiplist.insert(key, key * 100)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 70) {  // 20% erase operations
                    erase_attempts.fetch_add(1);
                    if (skiplist.erase(key)) {
                        successful_erases.fetch_add(1);
                    }
                } else {  // 30% find operations
                    find_attempts.fetch_add(1);
                    int value;
                    if (skiplist.find(key, value)) {
                        successful_finds.fetch_add(1);
                        assert(value == key * 100);
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
    std::cout << "Erase attempts: " << erase_attempts.load() 
              << ", successful: " << successful_erases.load() << "\n";
    std::cout << "Find attempts: " << find_attempts.load() 
              << ", successful: " << successful_finds.load() << "\n";
    std::cout << "Final skiplist size: " << skiplist.size() << "\n";
    
    // Verify skiplist integrity
    int expected_size = successful_inserts.load() - successful_erases.load();
    assert(skiplist.size() == expected_size);
    
    std::cout << "Stress operations test passed!\n";
}

void test_range_properties() {
    std::cout << "Testing range query properties...\n";
    
    AtomicSkipList<int, std::string> skiplist;
    
    // Insert data with gaps
    std::vector<int> keys = {10, 20, 30, 50, 70, 80, 90};
    for (int key : keys) {
        skiplist.insert(key, "value" + std::to_string(key));
    }
    
    // Test range queries using iteration
    auto range_query = [&](int start, int end) {
        std::vector<int> found_keys;
        for (auto it = skiplist.begin(); it != skiplist.end(); ++it) {
            auto [key, value] = *it;
            if (key >= start && key <= end) {
                found_keys.push_back(key);
            } else if (key > end) {
                break; // Skip list is ordered
            }
        }
        return found_keys;
    };
    
    // Test various ranges
    auto range1 = range_query(15, 35);  // Should find {20, 30}
    assert(range1.size() == 2);
    assert(range1[0] == 20 && range1[1] == 30);
    
    auto range2 = range_query(60, 85);  // Should find {70, 80}
    assert(range2.size() == 2);
    assert(range2[0] == 70 && range2[1] == 80);
    
    auto range3 = range_query(5, 15);   // Should find {10}
    assert(range3.size() == 1);
    assert(range3[0] == 10);
    
    auto range4 = range_query(95, 100); // Should find nothing
    assert(range4.empty());
    
    std::cout << "Range query properties test passed!\n";
}

int main() {
    std::cout << "AtomicSkipList Tests\n";
    std::cout << "====================\n\n";
    
    test_basic_skiplist_operations();
    test_ordered_insertion();
    test_erase_operations();
    test_string_keys();
    test_concurrent_skiplist_operations();
    test_emplace_operations();
    test_iteration();
    test_move_semantics();
    test_stress_operations();
    test_range_properties();
    
    std::cout << "\nAll skiplist tests passed!\n";
    std::cout << "\nNote: This SkipList implementation provides lock-free operations\n";
    std::cout << "with probabilistic balancing and maintains sorted order with\n";
    std::cout << "O(log n) expected performance for all operations.\n";
    
    return 0;
}