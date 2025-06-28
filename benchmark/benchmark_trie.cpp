#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_trie.hpp"

using namespace lockfree;

// Mutex-based trie for comparison
class MutexTrie {
private:
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        bool is_end_of_word = false;
    };
    
    std::unique_ptr<TrieNode> root_;
    mutable std::mutex mutex_;
    size_t size_;
    
    bool insert_recursive(TrieNode* node, const std::string& word, size_t index) {
        if (index == word.length()) {
            if (node->is_end_of_word) {
                return false; // Already exists
            }
            node->is_end_of_word = true;
            return true;
        }
        
        char c = word[index];
        if (node->children.find(c) == node->children.end()) {
            node->children[c] = std::make_unique<TrieNode>();
        }
        
        return insert_recursive(node->children[c].get(), word, index + 1);
    }
    
    bool contains_recursive(TrieNode* node, const std::string& word, size_t index) const {
        if (index == word.length()) {
            return node->is_end_of_word;
        }
        
        char c = word[index];
        auto it = node->children.find(c);
        if (it == node->children.end()) {
            return false;
        }
        
        return contains_recursive(it->second.get(), word, index + 1);
    }
    
    void collect_words_with_prefix(TrieNode* node, const std::string& prefix,
                                  std::string& current_word,
                                  std::vector<std::string>& result) const {
        if (node->is_end_of_word) {
            result.push_back(current_word);
        }
        
        for (const auto& [c, child] : node->children) {
            current_word.push_back(c);
            collect_words_with_prefix(child.get(), prefix, current_word, result);
            current_word.pop_back();
        }
    }
    
public:
    MutexTrie() : root_(std::make_unique<TrieNode>()), size_(0) {}
    
    bool insert(const std::string& word) {
        if (word.empty()) return false;
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (insert_recursive(root_.get(), word, 0)) {
            ++size_;
            return true;
        }
        return false;
    }
    
    bool contains(const std::string& word) const {
        if (word.empty()) return false;
        
        std::lock_guard<std::mutex> lock(mutex_);
        return contains_recursive(root_.get(), word, 0);
    }
    
    bool starts_with(const std::string& prefix) const {
        if (prefix.empty()) return size_ > 0;
        
        std::lock_guard<std::mutex> lock(mutex_);
        TrieNode* current = root_.get();
        
        for (char c : prefix) {
            auto it = current->children.find(c);
            if (it == current->children.end()) {
                return false;
            }
            current = it->second.get();
        }
        
        return true;
    }
    
    std::vector<std::string> get_all_with_prefix(const std::string& prefix) const {
        std::vector<std::string> result;
        std::lock_guard<std::mutex> lock(mutex_);
        
        TrieNode* current = root_.get();
        
        // Navigate to prefix node
        for (char c : prefix) {
            auto it = current->children.find(c);
            if (it == current->children.end()) {
                return result; // Empty result
            }
            current = it->second.get();
        }
        
        // Collect all words
        std::string current_word = prefix;
        collect_words_with_prefix(current, prefix, current_word, result);
        
        return result;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }
};

template<typename TrieType>
void benchmark_trie_throughput(const std::string& name, int num_threads, int operations_per_thread) {
    TrieType trie;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all test data to avoid string operations during timing
    constexpr int word_pool_size = 100000;
    std::vector<std::string> pre_generated_words(word_pool_size);
    std::vector<std::string> pre_generated_prefixes(word_pool_size);
    std::vector<int> pre_generated_ops(word_pool_size);
    
    // Initialize outside timing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> length_dist(5, 12);
    std::uniform_int_distribution<> char_dist('a', 'z');
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < word_pool_size; ++i) {
        // Generate word
        std::string word;
        int length = length_dist(gen);
        for (int j = 0; j < length; ++j) {
            word += static_cast<char>(char_dist(gen));
        }
        word += std::to_string(i); // Make unique
        pre_generated_words[i] = word;
        
        // Generate prefix (first 3 chars)
        pre_generated_prefixes[i] = word.substr(0, 3);
        
        // Generate operation type
        pre_generated_ops[i] = op_dist(gen);
    }
    
    std::cout << "Benchmarking " << name << " - " << num_threads << " threads, "
              << operations_per_thread << " ops each\n";
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                // Spin wait
            }
            
            int word_index = t * 1000;  // Different starting point per thread
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int idx = (word_index + i) % word_pool_size;
                const std::string& word = pre_generated_words[idx];
                int op = pre_generated_ops[idx];
                
                if (op < 50) {
                    // Insert operation (50%)
                    trie.insert(word);
                } else if (op < 80) {
                    // Lookup operation (30%)
                    trie.contains(word);
                } else {
                    // Prefix search operation (20%)
                    const std::string& prefix = pre_generated_prefixes[idx];
                    trie.starts_with(prefix);
                }
            }
        });
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    int total_operations = num_threads * operations_per_thread;
    double throughput = (total_operations * 1000000.0) / duration.count();
    
    std::cout << "  Time: " << duration.count() << " μs\n";
    std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
    std::cout << "  Final trie size: " << trie.size() << "\n\n";
}

void benchmark_insert_heavy_workload() {
    std::cout << "=== Insert-Heavy Workload (80% inserts) ===\n\n";
    
    benchmark_trie_throughput<AtomicTrie<char>>("Lock-free Trie", 8, 10000);
    benchmark_trie_throughput<MutexTrie>("Mutex Trie", 8, 10000);
}

void benchmark_lookup_heavy_workload() {
    std::cout << "=== Lookup-Heavy Workload ===\n\n";
    
    benchmark_trie_throughput<AtomicTrie<char>>("Lock-free Trie", 8, 10000);
    benchmark_trie_throughput<MutexTrie>("Mutex Trie", 8, 10000);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 5000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_trie_throughput<AtomicTrie<char>>("Lock-free Trie", threads, ops_per_thread);
        benchmark_trie_throughput<MutexTrie>("Mutex Trie", threads, ops_per_thread);
    }
}

int main() {
    std::cout << "Trie Performance Benchmark\n";
    std::cout << "==========================\n\n";
    
    benchmark_scaling();
    benchmark_insert_heavy_workload();
    benchmark_lookup_heavy_workload();
    
    return 0;
} 