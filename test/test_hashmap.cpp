#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <set>
#include <string>
#include <random>
#include <unordered_set>
#include "lockfree/atomic_hashmap.hpp"

using namespace lockfree;

void test_basic_hashmap_operations() {
    std::cout << "Testing basic hashmap operations...\n";
    
    AtomicHashMap<int, std::string> map;
    
    assert(map.empty());
    assert(map.size() == 0);
    assert(map.bucket_count() > 0);
    assert(map.load_factor() == 0.0);
    
    // Test insert
    assert(map.insert(1, "one"));
    assert(map.insert(2, "two"));
    assert(map.insert(3, "three"));
    
    assert(!map.empty());
    assert(map.size() == 3);
    assert(map.load_factor() > 0.0);
    
    // Test duplicate insert
    assert(!map.insert(2, "TWO"));
    assert(map.size() == 3);
    
    // Test find
    std::string value;
    assert(map.find(1, value));
    assert(value == "one");
    
    assert(map.find(2, value));
    assert(value == "two");
    
    assert(map.find(3, value));
    assert(value == "three");
    
    // Test find non-existent
    assert(!map.find(4, value));
    assert(!map.find(0, value));
    
    std::cout << "Basic hashmap operations test passed!\n";
}

void test_string_keys() {
    std::cout << "Testing string key operations...\n";
    
    AtomicHashMap<std::string, int> map;
    
    std::vector<std::pair<std::string, int>> data = {
        {"apple", 1}, {"banana", 2}, {"cherry", 3}, {"date", 4}
    };
    
    // Insert data
    for (const auto& [key, val] : data) {
        assert(map.insert(key, val));
    }
    
    assert(map.size() == data.size());
    
    // Verify all insertions
    for (const auto& [key, expected_val] : data) {
        int found_val;
        assert(map.find(key, found_val));
        assert(found_val == expected_val);
    }
    
    // Test non-existent keys
    int value;
    assert(!map.find("grape", value));
    assert(!map.find("", value));
    
    std::cout << "String key operations test passed!\n";
}

void test_concurrent_hashmap_operations() {
    std::cout << "Testing concurrent hashmap operations...\n";
    
    AtomicHashMap<int, int> map;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 500;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_lookups{0};
    std::atomic<int> failed_lookups{0};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/lookup threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 200);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                
                if (op_dist(gen) < 70) {  // 70% insert operations
                    int value = t * operations_per_thread + i;
                    if (map.insert(key, value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else {  // 30% lookup operations
                    int found_value;
                    if (map.find(key, found_value)) {
                        successful_lookups.fetch_add(1);
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
    std::cout << "Final map size: " << map.size() << "\n";
    
    // Basic consistency check
    assert(map.size() <= successful_inserts.load());
    
    std::cout << "Concurrent hashmap operations test passed!\n";
}

void test_erase_operations() {
    std::cout << "Testing erase operations...\n";
    
    AtomicHashMap<int, std::string> map;
    
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        map.insert(i, "value" + std::to_string(i));
    }
    
    assert(map.size() == 10);
    
    // Test successful erases
    assert(map.erase(5));
    assert(map.size() == 9);
    
    assert(map.erase(1));
    assert(map.erase(10));
    assert(map.size() == 7);
    
    // Test erase non-existent keys
    assert(!map.erase(5));  // Already erased
    assert(!map.erase(15)); // Never existed
    assert(map.size() == 7);
    
    // Verify remaining keys
    std::string value;
    assert(!map.find(1, value));  // Erased
    assert(!map.find(5, value));  // Erased
    assert(!map.find(10, value)); // Erased
    
    assert(map.find(2, value));   // Still there
    assert(map.find(9, value));   // Still there
    
    std::cout << "Erase operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicHashMap<int, std::pair<std::string, int>> map;
    
    assert(map.emplace(1, "first", 100));
    assert(map.emplace(2, "second", 200));
    assert(map.emplace(3, "third", 300));
    
    // Test duplicate emplace
    assert(!map.emplace(2, "SECOND", 999));
    
    assert(map.size() == 3);
    
    // Verify emplaced values
    std::pair<std::string, int> value;
    assert(map.find(1, value));
    assert(value.first == "first" && value.second == 100);
    
    assert(map.find(2, value));
    assert(value.first == "second" && value.second == 200);
    
    std::cout << "Emplace operations test passed!\n";
}

void test_iteration() {
    std::cout << "Testing hashmap iteration...\n";
    
    AtomicHashMap<int, std::string> map;
    
    std::set<int> inserted_keys;
    
    // Insert test data
    for (int i = 1; i <= 20; ++i) {
        if (i % 3 != 0) {  // Skip some keys to make it interesting
            map.insert(i, "value" + std::to_string(i));
            inserted_keys.insert(i);
        }
    }
    
    // Iterate and collect keys
    std::set<int> iterated_keys;
    int iteration_count = 0;
    
    for (auto it = map.begin(); it != map.end(); ++it) {
        auto [key, value] = *it;
        iterated_keys.insert(key);
        iteration_count++;
        
        // Verify value matches expected pattern
        assert(value == "value" + std::to_string(key));
        
        if (iteration_count > 50) break; // Safety limit
    }
    
    // Verify all inserted keys were found during iteration
    assert(iterated_keys == inserted_keys);
    assert(iteration_count == map.size());
    
    std::cout << "HashMap iteration test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicHashMap<int, std::unique_ptr<int>> map;
    
    // Test move insert
    assert(map.insert(1, std::make_unique<int>(100)));
    assert(map.insert(2, std::make_unique<int>(200)));
    
    // Test contains for move-only types
    assert(map.contains(1));
    assert(map.contains(2));
    assert(!map.contains(3));
    
    // Test find_if for move-only types
    bool found1 = map.find_if(1, [](const std::unique_ptr<int>& value) {
        assert(*value == 100);
        return true;  // Return true to indicate the condition was met
    });
    assert(found1);
    
    bool found2 = map.find_if(2, [](const std::unique_ptr<int>& value) {
        assert(*value == 200);
        return true;  // Return true to indicate the condition was met
    });
    assert(found2);
    
    bool found3 = map.find_if(3, [](const std::unique_ptr<int>& value) {
        assert(false); // Should not be called
        return false;  // This should never be reached
    });
    assert(!found3);
    
    std::cout << "Move semantics test passed!\n";
}

void test_load_factor_behavior() {
    std::cout << "Testing load factor behavior...\n";
    
    AtomicHashMap<int, int> map(8);  // Start with small bucket count
    
    double initial_load_factor = map.load_factor();
    assert(initial_load_factor == 0.0);
    
    // Insert items and monitor load factor
    for (int i = 0; i < 20; ++i) {
        map.insert(i, i * 10);
        
        double current_load_factor = map.load_factor();
        size_t current_size = map.size();
        size_t current_bucket_count = map.bucket_count();
        
        // Load factor should equal size / bucket_count
        double expected_load_factor = static_cast<double>(current_size) / current_bucket_count;
        assert(std::abs(current_load_factor - expected_load_factor) < 0.001);
        
        if (i % 5 == 0) {
            std::cout << "  Size: " << current_size 
                      << ", Buckets: " << current_bucket_count
                      << ", Load Factor: " << current_load_factor << "\n";
        }
    }
    
    std::cout << "Load factor behavior test passed!\n";
}

void test_stress_operations() {
    std::cout << "Testing stress operations...\n";
    
    AtomicHashMap<int, int> map;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    
    std::atomic<int> insert_attempts{0};
    std::atomic<int> successful_inserts{0};
    std::atomic<int> erase_attempts{0};
    std::atomic<int> successful_erases{0};
    std::atomic<int> lookup_attempts{0};
    std::atomic<int> successful_lookups{0};
    
    std::vector<std::thread> threads;
    
    // Stress test with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 300);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op < 50) {  // 50% insert operations
                    insert_attempts.fetch_add(1);
                    if (map.insert(key, key * 100)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 70) {  // 20% erase operations
                    erase_attempts.fetch_add(1);
                    if (map.erase(key)) {
                        successful_erases.fetch_add(1);
                    }
                } else {  // 30% lookup operations
                    lookup_attempts.fetch_add(1);
                    int value;
                    if (map.find(key, value)) {
                        successful_lookups.fetch_add(1);
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
    std::cout << "Lookup attempts: " << lookup_attempts.load() 
              << ", successful: " << successful_lookups.load() << "\n";
    std::cout << "Final map size: " << map.size() << "\n";
    
    // Verify map integrity
    int expected_size = successful_inserts.load() - successful_erases.load();
    assert(map.size() == expected_size);
    
    std::cout << "Stress operations test passed!\n";
}

int main() {
    std::cout << "AtomicHashMap Tests\n";
    std::cout << "===================\n\n";
    
    test_basic_hashmap_operations();
    test_string_keys();
    test_concurrent_hashmap_operations();
    test_erase_operations();
    test_emplace_operations();
    test_iteration();
    test_move_semantics();
    test_load_factor_behavior();
    test_stress_operations();
    
    std::cout << "\nAll hashmap tests passed!\n";
    std::cout << "\nNote: This HashMap implementation demonstrates basic lock-free\n";
    std::cout << "operations with chaining. A production implementation would include\n";
    std::cout << "more sophisticated resize and load balancing algorithms.\n";
    
    return 0;
}