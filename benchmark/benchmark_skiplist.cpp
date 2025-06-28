#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_skiplist.hpp"

using namespace lockfree;

// Mutex-based skip list for comparison
template<typename Key, typename Value>
class MutexSkipList {
private:
    struct Node {
        Key key;
        Value value;
        std::vector<Node*> next;
        
        Node(const Key& k, const Value& v, int level) 
            : key(k), value(v), next(level + 1, nullptr) {}
    };
    
    static constexpr int MAX_LEVEL = 16;
    Node* head_;
    mutable std::mutex mutex_;
    std::atomic<size_t> size_;
    std::mt19937 rng_;
    
    int random_level() {
        int level = 0;
        while (level < MAX_LEVEL - 1 && rng_() % 2 == 0) {
            level++;
        }
        return level;
    }
    
public:
    MutexSkipList() : size_(0), rng_(std::random_device{}()) {
        head_ = new Node(Key{}, Value{}, MAX_LEVEL - 1);
    }
    
    ~MutexSkipList() {
        std::lock_guard<std::mutex> lock(mutex_);
        Node* current = head_;
        while (current) {
            Node* next = current->next[0];
            delete current;
            current = next;
        }
    }
    
    bool insert(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Node*> predecessors(MAX_LEVEL);
        Node* current = head_;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            while (current->next[level] && current->next[level]->key < key) {
                current = current->next[level];
            }
            predecessors[level] = current;
        }
        
        // Check if key already exists
        if (current->next[0] && current->next[0]->key == key) {
            return false;
        }
        
        int level = random_level();
        Node* new_node = new Node(key, value, level);
        
        for (int i = 0; i <= level; ++i) {
            new_node->next[i] = predecessors[i]->next[i];
            predecessors[i]->next[i] = new_node;
        }
        
        size_.fetch_add(1);
        return true;
    }
    
    bool find(const Key& key, Value& result) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Node* current = head_;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            while (current->next[level] && current->next[level]->key < key) {
                current = current->next[level];
            }
        }
        
        current = current->next[0];
        if (current && current->key == key) {
            result = current->value;
            return true;
        }
        return false;
    }
    
    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Node*> predecessors(MAX_LEVEL);
        Node* current = head_;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            while (current->next[level] && current->next[level]->key < key) {
                current = current->next[level];
            }
            predecessors[level] = current;
        }
        
        Node* victim = current->next[0];
        if (!victim || victim->key != key) {
            return false;
        }
        
        for (int i = 0; i < static_cast<int>(victim->next.size()); ++i) {
            predecessors[i]->next[i] = victim->next[i];
        }
        
        delete victim;
        size_.fetch_sub(1);
        return true;
    }
    
    size_t size() const {
        return size_.load();
    }
    
    bool empty() const {
        return size_.load() == 0;
    }
};

template<typename SkipListType>
void benchmark_skiplist_throughput(const std::string& name, int num_threads, 
                                  int operations_per_thread, int read_percentage) {
    SkipListType skiplist;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all random data to avoid RNG during timing
    constexpr int key_pool_size = 100000;
    std::vector<int> pre_generated_keys(key_pool_size);
    std::vector<int> pre_generated_values(key_pool_size);
    std::vector<int> pre_generated_ops(key_pool_size);
    
    // Initialize outside timing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(1, operations_per_thread * num_threads / 2);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < key_pool_size; ++i) {
        pre_generated_keys[i] = key_dist(gen);
        pre_generated_values[i] = i;
        pre_generated_ops[i] = op_dist(gen);
    }
    
    std::cout << "Benchmarking " << name << " - " << num_threads << " threads, "
              << operations_per_thread << " ops each, " << read_percentage << "% reads\n";
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                // Spin wait
            }
            
            int key_index = t * 1000;  // Different starting point per thread
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int idx = (key_index + i) % key_pool_size;
                int key = pre_generated_keys[idx];
                int op = pre_generated_ops[idx];
                
                if (op < read_percentage) {
                    // Read operation
                    int value;
                    skiplist.find(key, value);
                } else if (op < read_percentage + 25) {
                    // Insert operation
                    int value = pre_generated_values[idx];
                    skiplist.insert(key, value);
                } else {
                    // Erase operation
                    skiplist.erase(key);
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
    std::cout << "  Final skiplist size: " << skiplist.size() << "\n\n";
}

void benchmark_read_heavy_workload() {
    std::cout << "=== Read-Heavy Workload (80% reads) ===\n\n";
    
    benchmark_skiplist_throughput<AtomicSkipList<int, int>>("Lock-free SkipList", 8, 10000, 80);
    benchmark_skiplist_throughput<MutexSkipList<int, int>>("Mutex SkipList", 8, 10000, 80);
}

void benchmark_write_heavy_workload() {
    std::cout << "=== Write-Heavy Workload (20% reads) ===\n\n";
    
    benchmark_skiplist_throughput<AtomicSkipList<int, int>>("Lock-free SkipList", 8, 10000, 20);
    benchmark_skiplist_throughput<MutexSkipList<int, int>>("Mutex SkipList", 8, 10000, 20);
}

void benchmark_balanced_workload() {
    std::cout << "=== Balanced Workload (50% reads) ===\n\n";
    
    benchmark_skiplist_throughput<AtomicSkipList<int, int>>("Lock-free SkipList", 8, 10000, 50);
    benchmark_skiplist_throughput<MutexSkipList<int, int>>("Mutex SkipList", 8, 10000, 50);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 5000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_skiplist_throughput<AtomicSkipList<int, int>>("Lock-free SkipList", threads, ops_per_thread, 60);
        benchmark_skiplist_throughput<MutexSkipList<int, int>>("Mutex SkipList", threads, ops_per_thread, 60);
    }
}

int main() {
    std::cout << "SkipList Performance Benchmark\n";
    std::cout << "==============================\n\n";
    
    benchmark_scaling();
    benchmark_read_heavy_workload();
    benchmark_write_heavy_workload();
    benchmark_balanced_workload();
    
    return 0;
} 