#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <memory>
#include "lockfree/atomic_trie.hpp"

using namespace lockfree;

void test_basic_operations() {
    std::cout << "Testing basic operations... ";
    
    AtomicStringTrie trie;
    
    // Test empty trie
    assert(trie.empty());
    assert(trie.size() == 0);
    assert(!trie.contains("test"));
    assert(!trie.starts_with("t"));
    
    // Test insertion
    assert(trie.insert("hello"));
    assert(!trie.empty());
    assert(trie.size() == 1);
    assert(trie.contains("hello"));
    assert(!trie.contains("hell"));
    assert(!trie.contains("helloworld"));
    
    // Test duplicate insertion
    assert(!trie.insert("hello"));
    assert(trie.size() == 1);
    
    // Test more insertions
    assert(trie.insert("world"));
    assert(trie.insert("help"));
    assert(trie.insert("helper"));
    assert(trie.size() == 4);
    
    // Test contains
    assert(trie.contains("hello"));
    assert(trie.contains("world"));
    assert(trie.contains("help"));
    assert(trie.contains("helper"));
    assert(!trie.contains("he"));
    assert(!trie.contains("helpers"));
    
    std::cout << "✓\n";
}

void test_prefix_operations() {
    std::cout << "Testing prefix operations... ";
    
    AtomicStringTrie trie;
    
    // Insert test data
    std::vector<std::string> words = {
        "app", "apple", "application", "apply", "approach",
        "car", "card", "care", "careful",
        "cat", "catch"
    };
    
    for (const auto& word : words) {
        assert(trie.insert(word));
    }
    
    // Test starts_with
    assert(trie.starts_with("app"));
    assert(trie.starts_with("car"));
    assert(trie.starts_with("ca"));
    assert(!trie.starts_with("xyz"));
    assert(!trie.starts_with("dog"));
    
    // Test get_all_with_prefix
    auto app_words = trie.get_all_with_prefix("app");
    assert(app_words.size() == 5);
    std::set<std::string> app_set(app_words.begin(), app_words.end());
    assert(app_set.count("app") == 1);
    assert(app_set.count("apple") == 1);
    assert(app_set.count("application") == 1);
    assert(app_set.count("apply") == 1);
    assert(app_set.count("approach") == 1);
    
    auto car_words = trie.get_all_with_prefix("car");
    assert(car_words.size() == 4);
    std::set<std::string> car_set(car_words.begin(), car_words.end());
    assert(car_set.count("car") == 1);
    assert(car_set.count("card") == 1);
    assert(car_set.count("care") == 1);
    assert(car_set.count("careful") == 1);
    
    auto cat_words = trie.get_all_with_prefix("cat");
    assert(cat_words.size() == 2);
    
    auto xyz_words = trie.get_all_with_prefix("xyz");
    assert(xyz_words.empty());
    
    // Test count_with_prefix
    assert(trie.count_with_prefix("app") == 5);
    assert(trie.count_with_prefix("car") == 4);
    assert(trie.count_with_prefix("cat") == 2);
    assert(trie.count_with_prefix("xyz") == 0);
    
    std::cout << "✓\n";
}

void test_erase_operations() {
    std::cout << "Testing erase operations... ";
    
    AtomicStringTrie trie;
    
    // Insert test data
    std::vector<std::string> words = {"test", "testing", "tester", "tea", "team"};
    for (const auto& word : words) {
        assert(trie.insert(word));
    }
    
    assert(trie.size() == 5);
    
    // Test successful erase
    assert(trie.erase("test"));
    assert(trie.size() == 4);
    assert(!trie.contains("test"));
    assert(trie.contains("testing"));
    assert(trie.contains("tester"));
    
    // Test erase non-existent word
    assert(!trie.erase("nonexistent"));
    assert(trie.size() == 4);
    
    // Test erase another word
    assert(trie.erase("testing"));
    assert(trie.size() == 3);
    assert(!trie.contains("testing"));
    assert(trie.contains("tester"));
    
    // Verify prefix search still works
    auto te_words = trie.get_all_with_prefix("te");
    assert(te_words.size() == 3); // tester, tea, team
    
    std::cout << "✓\n";
}

void test_iterator() {
    std::cout << "Testing iterator... ";
    
    AtomicStringTrie trie;
    
    // Test empty trie iterator
    assert(trie.begin() == trie.end());
    
    // Insert words
    std::vector<std::string> words = {"zebra", "apple", "banana", "cat", "dog"};
    for (const auto& word : words) {
        assert(trie.insert(word));
    }
    
    // Collect words via iterator
    std::vector<std::string> iterated_words;
    for (const auto& word : trie) {
        iterated_words.push_back(word);
    }
    
    // Should be in lexicographic order
    std::vector<std::string> expected = {"apple", "banana", "cat", "dog", "zebra"};
    assert(iterated_words == expected);
    
    // Test iterator comparison
    auto it1 = trie.begin();
    auto it2 = trie.begin();
    auto it_end = trie.end();
    
    assert(it1 == it2);
    assert(it1 != it_end);
    
    // Test iterator increment
    ++it1;
    assert(it1 != it2);
    if (it1 != it_end) {
        assert(*it1 == "banana");
    }
    
    std::cout << "✓\n";
}

void test_longest_prefix() {
    std::cout << "Testing longest prefix... ";
    
    AtomicStringTrie trie;
    
    // Insert words
    std::vector<std::string> words = {"app", "apple", "application"};
    for (const auto& word : words) {
        assert(trie.insert(word));
    }
    
    // Test longest prefix matching
    assert(trie.longest_prefix("apple") == "apple");
    assert(trie.longest_prefix("application") == "application");
    assert(trie.longest_prefix("applications") == "application");
    assert(trie.longest_prefix("app") == "app");
    assert(trie.longest_prefix("appl") == "app");
    assert(trie.longest_prefix("xyz") == "");
    assert(trie.longest_prefix("a") == "");
    
    std::cout << "✓\n";
}

void test_edge_cases() {
    std::cout << "Testing edge cases... ";
    
    AtomicStringTrie trie;
    
    // Test empty string (should not be allowed)
    assert(!trie.insert(""));
    assert(!trie.contains(""));
    assert(!trie.erase(""));
    
    // Test single character
    assert(trie.insert("a"));
    assert(trie.contains("a"));
    assert(trie.starts_with("a"));
    
    // Test very long string
    std::string long_string(1000, 'x');
    assert(trie.insert(long_string));
    assert(trie.contains(long_string));
    
    // Test special characters
    assert(trie.insert("hello@world"));
    assert(trie.insert("test123"));
    assert(trie.insert("spa ce"));
    assert(trie.contains("hello@world"));
    assert(trie.contains("test123"));
    assert(trie.contains("spa ce"));
    
    // Test prefix with special characters
    assert(trie.starts_with("hello@"));
    assert(trie.starts_with("test"));
    assert(trie.starts_with("spa"));
    
    std::cout << "✓\n";
}

void test_concurrent_operations() {
    std::cout << "Testing concurrent operations... ";
    
    AtomicStringTrie trie;
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_contains{0};
    std::atomic<int> successful_prefix_searches{0};
    
    // Launch threads with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 99);
            std::uniform_int_distribution<> length_dist(3, 8);
            std::uniform_int_distribution<> char_dist('a', 'z');
            
            for (int i = 0; i < operations_per_thread; ++i) {
                // Generate test string
                std::string test_word;
                int length = length_dist(gen);
                for (int j = 0; j < length; ++j) {
                    test_word += static_cast<char>(char_dist(gen));
                }
                test_word += std::to_string(t * operations_per_thread + i); // Make unique
                
                int op = op_dist(gen);
                
                if (op < 60) {
                    // Insert operation (60%)
                    if (trie.insert(test_word)) {
                        successful_inserts.fetch_add(1);
                    }
                } else if (op < 85) {
                    // Contains operation (25%)
                    if (trie.contains(test_word)) {
                        successful_contains.fetch_add(1);
                    }
                } else {
                    // Prefix search operation (15%)
                    std::string prefix = test_word.substr(0, 2);
                    if (trie.starts_with(prefix)) {
                        auto words = trie.get_all_with_prefix(prefix);
                        successful_prefix_searches.fetch_add(words.size());
                    }
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify final state
    assert(successful_inserts.load() > 0);
    assert(trie.size() == static_cast<size_t>(successful_inserts.load()));
    
    std::cout << "✓ (inserts: " << successful_inserts.load() 
              << ", contains: " << successful_contains.load()
              << ", prefix_searches: " << successful_prefix_searches.load() << ")\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics... ";
    
    AtomicStringTrie trie;
    
    // Test move insert
    std::string movable_word = "movable";
    std::string copy_of_word = movable_word;
    
    assert(trie.insert(std::move(movable_word)));
    assert(trie.contains(copy_of_word));
    assert(trie.size() == 1);
    
    // Test emplace
    assert(trie.emplace("emplaced_word"));
    assert(trie.contains("emplaced_word"));
    assert(trie.size() == 2);
    
    std::cout << "✓\n";
}

void test_stress_operations() {
    std::cout << "Testing stress operations... ";
    
    AtomicStringTrie trie;
    constexpr int num_words = 1000;
    
    std::vector<std::string> words;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> length_dist(5, 15);
    std::uniform_int_distribution<> char_dist('a', 'z');
    
    // Generate unique test words
    for (int i = 0; i < num_words; ++i) {
        std::string word;
        int length = length_dist(gen);
        for (int j = 0; j < length; ++j) {
            word += static_cast<char>(char_dist(gen));
        }
        word += std::to_string(i); // Ensure uniqueness
        words.push_back(word);
    }
    
    // Insert all words
    for (const auto& word : words) {
        assert(trie.insert(word));
    }
    
    assert(trie.size() == num_words);
    
    // Verify all words are present
    for (const auto& word : words) {
        assert(trie.contains(word));
    }
    
    // Test prefix searches on random prefixes
    int prefix_tests = 100;
    for (int i = 0; i < prefix_tests; ++i) {
        std::string prefix;
        int prefix_length = std::uniform_int_distribution<>(1, 3)(gen);
        for (int j = 0; j < prefix_length; ++j) {
            prefix += static_cast<char>(char_dist(gen));
        }
        
        auto matching_words = trie.get_all_with_prefix(prefix);
        size_t expected_count = std::count_if(words.begin(), words.end(),
            [&prefix](const std::string& word) {
                return word.substr(0, prefix.length()) == prefix;
            });
        
        assert(matching_words.size() == expected_count);
    }
    
    // Test iterator with all words
    size_t count = 0;
    for (const auto& word : trie) {
        (void)word; // Suppress unused variable warning
        ++count;
    }
    assert(count == num_words);
    
    std::cout << "✓ (" << num_words << " words)\n";
}

void test_performance_characteristics() {
    std::cout << "Testing performance characteristics... ";
    
    AtomicStringTrie trie;
    constexpr int num_operations = 10000;
    
    // Generate test data
    std::vector<std::string> words;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> length_dist(5, 10);
    std::uniform_int_distribution<> char_dist('a', 'z');
    
    for (int i = 0; i < num_operations; ++i) {
        std::string word;
        int length = length_dist(gen);
        for (int j = 0; j < length; ++j) {
            word += static_cast<char>(char_dist(gen));
        }
        word += std::to_string(i);
        words.push_back(word);
    }
    
    // Measure insertion time
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& word : words) {
        trie.insert(word);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Measure lookup time
    start = std::chrono::high_resolution_clock::now();
    for (const auto& word : words) {
        assert(trie.contains(word));
    }
    end = std::chrono::high_resolution_clock::now();
    auto lookup_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Measure prefix search time
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        std::string prefix = words[i].substr(0, 3);
        auto results = trie.get_all_with_prefix(prefix);
        (void)results; // Suppress unused variable warning
    }
    end = std::chrono::high_resolution_clock::now();
    auto prefix_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double insert_rate = (num_operations * 1000000.0) / insert_duration.count();
    double lookup_rate = (num_operations * 1000000.0) / lookup_duration.count();
    double prefix_rate = (1000 * 1000000.0) / prefix_duration.count();
    
    std::cout << "✓\n";
    std::cout << "  Insert rate: " << static_cast<int>(insert_rate) << " ops/sec\n";
    std::cout << "  Lookup rate: " << static_cast<int>(lookup_rate) << " ops/sec\n";
    std::cout << "  Prefix search rate: " << static_cast<int>(prefix_rate) << " ops/sec\n";
}

int main() {
    std::cout << "AtomicTrie Test Suite\n";
    std::cout << "====================\n\n";
    
    try {
        test_basic_operations();
        test_prefix_operations();
        test_erase_operations();
        test_iterator();
        test_longest_prefix();
        test_edge_cases();
        test_concurrent_operations();
        test_move_semantics();
        test_stress_operations();
        test_performance_characteristics();
        
        std::cout << "\n✅ All tests passed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
} 