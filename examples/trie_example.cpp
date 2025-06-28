#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <string>
#include "lockfree/atomic_trie.hpp"

using namespace lockfree;

void demonstrate_basic_operations() {
    std::cout << "=== Basic Trie Operations ===\n\n";
    
    AtomicStringTrie trie;
    
    // Insert words
    std::vector<std::string> words = {
        "apple", "app", "application", "apply", "banana", "band", "bandana",
        "cat", "car", "card", "care", "careful", "dog", "door", "down"
    };
    
    std::cout << "Inserting words: ";
    for (const auto& word : words) {
        std::cout << word << " ";
        trie.insert(word);
    }
    std::cout << "\n";
    std::cout << "Total words inserted: " << trie.size() << "\n\n";
    
    // Test contains
    std::cout << "Testing contains:\n";
    std::vector<std::string> test_words = {"app", "apple", "application", "cat", "xyz", "car"};
    for (const auto& word : test_words) {
        std::cout << "  \"" << word << "\": " << (trie.contains(word) ? "✓" : "✗") << "\n";
    }
    std::cout << "\n";
    
    // Test prefix search
    std::cout << "Testing prefix search:\n";
    std::vector<std::string> prefixes = {"app", "car", "ban", "do"};
    for (const auto& prefix : prefixes) {
        std::cout << "  Words starting with \"" << prefix << "\": ";
        if (trie.starts_with(prefix)) {
            auto words_with_prefix = trie.get_all_with_prefix(prefix);
            for (const auto& word : words_with_prefix) {
                std::cout << word << " ";
            }
        } else {
            std::cout << "(none)";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void demonstrate_prefix_operations() {
    std::cout << "=== Prefix Operations ===\n\n";
    
    AtomicStringTrie trie;
    
    // Insert programming terms
    std::vector<std::string> programming_terms = {
        "algorithm", "array", "atomic", "boolean", "byte", "char", "class",
        "const", "constructor", "destructor", "exception", "function",
        "inheritance", "interface", "iterator", "memory", "method",
        "namespace", "object", "pointer", "reference", "string", "template",
        "thread", "variable", "vector"
    };
    
    for (const auto& term : programming_terms) {
        trie.insert(term);
    }
    
    std::cout << "Inserted " << programming_terms.size() << " programming terms\n\n";
    
    // Test various prefix searches
    std::vector<std::string> search_prefixes = {"a", "co", "in", "m", "th"};
    
    for (const auto& prefix : search_prefixes) {
        auto matches = trie.get_all_with_prefix(prefix);
        std::cout << "Terms starting with \"" << prefix << "\" (" << matches.size() << " found):\n";
        for (const auto& match : matches) {
            std::cout << "  " << match << "\n";
        }
        std::cout << "\n";
    }
    
    // Test longest prefix
    std::cout << "Testing longest prefix matching:\n";
    std::vector<std::string> test_strings = {"algorithm123", "construction", "memory_leak", "xyz"};
    for (const auto& test_str : test_strings) {
        auto longest = trie.longest_prefix(test_str);
        std::cout << "  \"" << test_str << "\" -> longest prefix: \"" << longest << "\"\n";
    }
    std::cout << "\n";
}

void demonstrate_iteration() {
    std::cout << "=== Iterator Support ===\n\n";
    
    AtomicStringTrie trie;
    
    // Insert some fruits
    std::vector<std::string> fruits = {
        "apple", "apricot", "banana", "blueberry", "cherry", "grape",
        "kiwi", "lemon", "mango", "orange", "peach", "strawberry"
    };
    
    for (const auto& fruit : fruits) {
        trie.insert(fruit);
    }
    
    std::cout << "All fruits in lexicographic order:\n";
    for (const auto& word : trie) {
        std::cout << "  " << word << "\n";
    }
    std::cout << "\n";
    
    // Demonstrate prefix counting
    std::cout << "Prefix statistics:\n";
    std::vector<std::string> prefixes = {"a", "b", "c", "s"};
    for (const auto& prefix : prefixes) {
        size_t count = trie.count_with_prefix(prefix);
        std::cout << "  Fruits starting with \"" << prefix << "\": " << count << "\n";
    }
    std::cout << "\n";
}

void demonstrate_erase_operations() {
    std::cout << "=== Erase Operations ===\n\n";
    
    AtomicStringTrie trie;
    
    // Insert colors
    std::vector<std::string> colors = {
        "red", "green", "blue", "yellow", "orange", "purple", "pink",
        "black", "white", "gray", "brown", "cyan", "magenta"
    };
    
    for (const auto& color : colors) {
        trie.insert(color);
    }
    
    std::cout << "Initial colors (" << trie.size() << " total):\n";
    for (const auto& color : trie) {
        std::cout << "  " << color << "\n";
    }
    std::cout << "\n";
    
    // Erase some colors
    std::vector<std::string> to_erase = {"red", "blue", "yellow", "nonexistent"};
    
    std::cout << "Erasing colors:\n";
    for (const auto& color : to_erase) {
        bool erased = trie.erase(color);
        std::cout << "  \"" << color << "\": " << (erased ? "✓ erased" : "✗ not found") << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Remaining colors (" << trie.size() << " total):\n";
    for (const auto& color : trie) {
        std::cout << "  " << color << "\n";
    }
    std::cout << "\n";
}

void demonstrate_concurrent_operations() {
    std::cout << "=== Concurrent Operations ===\n\n";
    
    AtomicStringTrie trie;
    constexpr int num_threads = 4;
    constexpr int words_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_inserts{0};
    std::atomic<int> successful_lookups{0};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Producer threads - insert words
    for (int t = 0; t < num_threads / 2; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> length_dist(3, 10);
            std::uniform_int_distribution<> char_dist('a', 'z');
            
            for (int i = 0; i < words_per_thread; ++i) {
                // Generate random word
                std::string word;
                int length = length_dist(gen);
                for (int j = 0; j < length; ++j) {
                    word += static_cast<char>(char_dist(gen));
                }
                word += std::to_string(t * words_per_thread + i); // Make unique
                
                if (trie.insert(word)) {
                    successful_inserts.fetch_add(1);
                }
            }
        });
    }
    
    // Consumer threads - lookup words and test prefixes
    for (int t = num_threads / 2; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> char_dist('a', 'z');
            
            // Give producers time to insert some words
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            for (int i = 0; i < words_per_thread; ++i) {
                // Test random prefix
                std::string prefix;
                prefix += static_cast<char>(char_dist(gen));
                prefix += static_cast<char>(char_dist(gen));
                
                if (trie.starts_with(prefix)) {
                    auto words = trie.get_all_with_prefix(prefix);
                    successful_lookups.fetch_add(words.size());
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Concurrent test completed in " << duration.count() << " ms\n";
    std::cout << "Successful inserts: " << successful_inserts.load() << "\n";
    std::cout << "Words found in prefix searches: " << successful_lookups.load() << "\n";
    std::cout << "Final trie size: " << trie.size() << "\n";
    std::cout << "Empty: " << (trie.empty() ? "yes" : "no") << "\n\n";
}

void demonstrate_autocomplete_simulation() {
    std::cout << "=== Autocomplete Simulation ===\n\n";
    
    AtomicStringTrie trie;
    
    // Insert common English words (simulated dictionary)
    std::vector<std::string> dictionary = {
        "the", "and", "for", "are", "but", "not", "you", "all", "can", "had",
        "her", "was", "one", "our", "out", "day", "get", "has", "him", "his",
        "how", "man", "new", "now", "old", "see", "two", "way", "who", "boy",
        "did", "its", "let", "put", "say", "she", "too", "use", "dad", "mom",
        "apple", "application", "apply", "approach", "appropriate", "approve",
        "car", "card", "care", "career", "careful", "carry", "case", "catch",
        "computer", "complete", "company", "compare", "competition", "condition"
    };
    
    for (const auto& word : dictionary) {
        trie.insert(word);
    }
    
    std::cout << "Dictionary loaded with " << trie.size() << " words\n\n";
    
    // Simulate autocomplete queries
    std::vector<std::string> queries = {"a", "ap", "app", "c", "ca", "car", "co", "com"};
    
    for (const auto& query : queries) {
        auto suggestions = trie.get_all_with_prefix(query);
        
        std::cout << "Autocomplete for \"" << query << "\":\n";
        if (suggestions.empty()) {
            std::cout << "  (no suggestions)\n";
        } else {
            // Limit to first 5 suggestions
            size_t limit = std::min(suggestions.size(), size_t(5));
            for (size_t i = 0; i < limit; ++i) {
                std::cout << "  " << suggestions[i] << "\n";
            }
            if (suggestions.size() > 5) {
                std::cout << "  ... and " << (suggestions.size() - 5) << " more\n";
            }
        }
        std::cout << "\n";
    }
}

void demonstrate_memory_usage() {
    std::cout << "=== Memory Usage Analysis ===\n\n";
    
    AtomicStringTrie trie;
    
    // Insert words of different lengths
    std::vector<std::pair<std::string, int>> test_sets = {
        {"Short words (3-4 chars)", 4},
        {"Medium words (5-7 chars)", 6},
        {"Long words (8-12 chars)", 10}
    };
    
    for (const auto& [description, avg_length] : test_sets) {
        AtomicStringTrie test_trie;
        
        // Generate words of specific length
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> char_dist('a', 'z');
        std::uniform_int_distribution<> length_dist(avg_length - 1, avg_length + 1);
        
        const int num_words = 1000;
        
        for (int i = 0; i < num_words; ++i) {
            std::string word;
            int length = length_dist(gen);
            for (int j = 0; j < length; ++j) {
                word += static_cast<char>(char_dist(gen));
            }
            word += std::to_string(i); // Ensure uniqueness
            test_trie.insert(word);
        }
        
        std::cout << description << ":\n";
        std::cout << "  Words inserted: " << test_trie.size() << "\n";
        std::cout << "  Sample prefix search (\"a\"): " << test_trie.count_with_prefix("a") << " matches\n";
        std::cout << "  Sample prefix search (\"ab\"): " << test_trie.count_with_prefix("ab") << " matches\n\n";
    }
}

int main() {
    std::cout << "AtomicTrie Example\n";
    std::cout << "==================\n\n";
    
    try {
        demonstrate_basic_operations();
        demonstrate_prefix_operations();
        demonstrate_iteration();
        demonstrate_erase_operations();
        demonstrate_concurrent_operations();
        demonstrate_autocomplete_simulation();
        demonstrate_memory_usage();
        
        std::cout << "=== Summary ===\n\n";
        std::cout << "AtomicTrie provides:\n";
        std::cout << "• Thread-safe prefix tree operations\n";
        std::cout << "• Efficient prefix search and autocomplete\n";
        std::cout << "• Lock-free concurrent access\n";
        std::cout << "• Memory-efficient string storage\n";
        std::cout << "• Iterator support for traversal\n";
        std::cout << "• Longest prefix matching\n\n";
        
        std::cout << "Perfect for:\n";
        std::cout << "• Autocomplete systems\n";
        std::cout << "• Dictionary/spell checkers\n";
        std::cout << "• IP routing tables\n";
        std::cout << "• String compression\n";
        std::cout << "• Text indexing\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 