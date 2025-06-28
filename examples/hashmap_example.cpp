#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include "lockfree/atomic_hashmap.hpp"

using namespace lockfree;

void test_basic_hashmap_operations() {
    std::cout << "=== Basic HashMap Operations ===\n";
    
    AtomicHashMap<std::string, int> map;
    
    std::cout << "Initial state - empty: " << map.empty() 
              << ", size: " << map.size() << "\n";
    
    // Insert key-value pairs
    std::cout << "Inserting key-value pairs...\n";
    assert(map.insert("apple", 100));
    assert(map.insert("banana", 200));
    assert(map.insert("cherry", 300));
    assert(map.insert("date", 400));
    
    std::cout << "After insertions - size: " << map.size() 
              << ", load factor: " << map.load_factor() << "\n";
    
    // Test duplicate insertion
    std::cout << "Testing duplicate insertion:\n";
    bool duplicate_result = map.insert("apple", 999);
    std::cout << "  Inserting duplicate 'apple': " 
              << (duplicate_result ? "Success" : "Failed (expected)") << "\n";
    
    // Test lookups
    std::cout << "\nTesting lookups:\n";
    int value;
    std::vector<std::string> keys = {"apple", "banana", "cherry", "date", "elderberry"};
    
    for (const auto& key : keys) {
        if (map.find(key, value)) {
            std::cout << "  " << key << " -> " << value << "\n";
        } else {
            std::cout << "  " << key << " -> not found\n";
        }
    }
    
    std::cout << "Final state - size: " << map.size() << "\n\n";
}

void test_concurrent_hashmap_operations() {
    std::cout << "=== Concurrent HashMap Operations ===\n";
    
    AtomicHashMap<int, std::string> map;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 250;
    
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_lookups{0};
    std::atomic<int> failed_lookups{0};
    std::atomic<bool> start_flag{false};
    
    std::vector<std::thread> threads;
    
    // Mixed insert/lookup threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 500);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                
                if (op_dist(gen) < 70) {  // 70% insert operations
                    std::string value = "Thread" + std::to_string(t) + "_Item" + std::to_string(i);
                    if (map.insert(key, value)) {
                        successful_inserts.fetch_add(1);
                    }
                } else {  // 30% lookup operations
                    std::string found_value;
                    if (map.find(key, found_value)) {
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
    std::cout << "Successful lookups: " << successful_lookups.load() << "\n";
    std::cout << "Failed lookups: " << failed_lookups.load() << "\n";
    std::cout << "Final map size: " << map.size() << "\n";
    std::cout << "Final load factor: " << map.load_factor() << "\n\n";
}

void test_string_to_string_mapping() {
    std::cout << "=== String-to-String Mapping ===\n";
    
    AtomicHashMap<std::string, std::string> dictionary;
    
    // Create a simple dictionary
    std::vector<std::pair<std::string, std::string>> translations = {
        {"hello", "hola"}, {"goodbye", "adiós"}, {"please", "por favor"},
        {"thank you", "gracias"}, {"yes", "sí"}, {"no", "no"},
        {"water", "agua"}, {"food", "comida"}, {"house", "casa"}
    };
    
    std::cout << "Building English-Spanish dictionary...\n";
    for (const auto& [english, spanish] : translations) {
        dictionary.insert(english, spanish);
    }
    
    std::cout << "Dictionary size: " << dictionary.size() << "\n";
    
    // Test lookups
    std::cout << "\nTranslations:\n";
    std::vector<std::string> words_to_translate = {
        "hello", "water", "thank you", "cat", "house"
    };
    
    for (const auto& word : words_to_translate) {
        std::string translation;
        if (dictionary.find(word, translation)) {
            std::cout << "  " << word << " -> " << translation << "\n";
        } else {
            std::cout << "  " << word << " -> translation not found\n";
        }
    }
    std::cout << "\n";
}

void test_cache_simulation() {
    std::cout << "=== Cache Simulation ===\n";
    
    AtomicHashMap<int, std::string> cache;
    constexpr int num_readers = 3;
    constexpr int num_writers = 2;
    constexpr int simulation_duration_ms = 1000;
    
    std::atomic<bool> keep_running{true};
    std::atomic<int> cache_hits{0};
    std::atomic<int> cache_misses{0};
    std::atomic<int> cache_writes{0};
    
    std::vector<std::thread> threads;
    
    // Writer threads (cache population)
    for (int w = 0; w < num_writers; ++w) {
        threads.emplace_back([&, w]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 100);
            std::uniform_int_distribution<> delay_dist(10, 100);
            
            while (keep_running.load()) {
                int key = key_dist(gen);
                std::string value = "CachedData_" + std::to_string(key) + "_Writer" + std::to_string(w);
                
                if (cache.insert(key, value)) {
                    cache_writes.fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            }
        });
    }
    
    // Reader threads (cache access)
    for (int r = 0; r < num_readers; ++r) {
        threads.emplace_back([&, r]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 100);
            std::uniform_int_distribution<> delay_dist(5, 50);
            
            while (keep_running.load()) {
                int key = key_dist(gen);
                std::string value;
                
                if (cache.find(key, value)) {
                    cache_hits.fetch_add(1);
                } else {
                    cache_misses.fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            }
        });
    }
    
    std::cout << "Running cache simulation for " << simulation_duration_ms << " ms...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(simulation_duration_ms));
    
    keep_running.store(false);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int total_reads = cache_hits.load() + cache_misses.load();
    double hit_rate = total_reads > 0 ? (100.0 * cache_hits.load()) / total_reads : 0.0;
    
    std::cout << "Cache simulation results:\n";
    std::cout << "  Cache writes: " << cache_writes.load() << "\n";
    std::cout << "  Cache hits: " << cache_hits.load() << "\n";
    std::cout << "  Cache misses: " << cache_misses.load() << "\n";
    std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) << hit_rate << "%\n";
    std::cout << "  Final cache size: " << cache.size() << "\n\n";
}

void test_emplace_operations() {
    std::cout << "=== Emplace Operations ===\n";
    
    AtomicHashMap<int, std::pair<std::string, int>> map;
    
    // Test emplace with pair construction
    map.emplace(1, "first", 100);
    map.emplace(2, "second", 200);
    map.emplace(3, "third", 300);
    
    std::cout << "Emplaced 3 pairs\n";
    std::cout << "Map size: " << map.size() << "\n";
    
    // Test lookups
    std::pair<std::string, int> value;
    for (int key = 1; key <= 3; ++key) {
        if (map.find(key, value)) {
            std::cout << "  Key " << key << " -> (" 
                      << value.first << ", " << value.second << ")\n";
        }
    }
    std::cout << "\n";
}

void test_iteration() {
    std::cout << "=== HashMap Iteration ===\n";
    
    AtomicHashMap<char, int> map;
    
    // Insert some data
    for (char c = 'a'; c <= 'j'; ++c) {
        map.insert(c, static_cast<int>(c - 'a' + 1));
    }
    
    std::cout << "Inserted " << map.size() << " character mappings\n";
    std::cout << "Iterating through map:\n";
    
    int count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        auto [key, value] = *it;
        std::cout << "  " << key << " -> " << value << "\n";
        ++count;
        if (count >= 10) break; // Limit output
    }
    
    std::cout << "Iterated through " << count << " elements\n\n";
}

int main() {
    std::cout << "Lock-free HashMap Example\n";
    std::cout << "=========================\n\n";
    
    test_basic_hashmap_operations();
    test_concurrent_hashmap_operations();
    test_string_to_string_mapping();
    test_cache_simulation();
    test_emplace_operations();
    test_iteration();
    
    std::cout << "All HashMap tests completed!\n";
    std::cout << "\nNote: This HashMap implementation uses chaining for collision\n";
    std::cout << "resolution and provides basic lock-free operations. A production\n";
    std::cout << "implementation would include more sophisticated resize algorithms.\n";
    
    return 0;
}