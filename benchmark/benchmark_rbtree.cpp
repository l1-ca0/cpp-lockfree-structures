#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <set>
#include <map>
#include <atomic>
#include <random>
#include <algorithm>
#include <string>
#include "lockfree/atomic_rbtree.hpp"

using namespace lockfree;

// Mutex-based RBTree (using std::map for key-value pairs)
template<typename Key, typename Value>
class MutexRBTree {
private:
    std::map<Key, Value> tree_;
    mutable std::mutex mutex_;
    
public:
    bool insert(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        return tree_.insert({key, value}).second;
    }
    
    bool remove(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return tree_.erase(key) > 0;
    }
    
    bool find(const Key& key, Value& result) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tree_.find(key);
        if (it != tree_.end()) {
            result = it->second;
            return true;
        }
        return false;
    }
    
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tree_.find(key) != tree_.end();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tree_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tree_.size();
    }
};

template<typename TreeType>
void benchmark_tree_throughput(const std::string& name, int num_threads, 
                              int operations_per_thread, int read_percentage) {
    TreeType tree;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all random data to avoid RNG during timing
    constexpr int value_pool_size = 100000;
    std::vector<int> pre_generated_keys(value_pool_size);
    std::vector<int> pre_generated_values(value_pool_size);
    std::vector<int> pre_generated_ops(value_pool_size);
    
    // Initialize outside timing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(1, 50000);
    std::uniform_int_distribution<> value_dist(1, 100000);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < value_pool_size; ++i) {
        pre_generated_keys[i] = key_dist(gen);
        pre_generated_values[i] = value_dist(gen);
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
            
            int index_offset = t * 1000;  // Different starting point per thread
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int idx = (index_offset + i) % value_pool_size;
                int key = pre_generated_keys[idx];
                int value = pre_generated_values[idx];
                int op = pre_generated_ops[idx];
                
                if (op < read_percentage) {
                    // Read operation (find)
                    int result;
                    tree.find(key, result);
                } else if (op < read_percentage + 30) {
                    // Insert operation
                    tree.insert(key, value);
                } else {
                    // Contains operation
                    tree.contains(key);
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
    std::cout << "  Final tree size: " << tree.size() << "\n\n";
}

template<typename TreeType>
void benchmark_tree_latency(const std::string& name, int num_operations) {
    TreeType tree;
    std::vector<std::chrono::nanoseconds> insert_times;
    std::vector<std::chrono::nanoseconds> find_times;
    
    insert_times.reserve(num_operations);
    find_times.reserve(num_operations);
    
    std::cout << "Latency benchmark for " << name << " (" << num_operations << " operations)\n";
    
    // Pre-populate for find operations
    for (int i = 0; i < num_operations / 2; ++i) {
        tree.insert(i, i * 10);
    }
    
    // Measure insert latency
    for (int i = num_operations / 2; i < num_operations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        tree.insert(i, i * 10);
        auto end = std::chrono::high_resolution_clock::now();
        insert_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    }
    
    // Measure find latency
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(0, num_operations - 1);
    
    for (int i = 0; i < num_operations; ++i) {
        int key = key_dist(gen);
        int value;
        
        auto start = std::chrono::high_resolution_clock::now();
        tree.find(key, value);
        auto end = std::chrono::high_resolution_clock::now();
        find_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    }
    
    // Calculate statistics
    auto calc_stats = [](std::vector<std::chrono::nanoseconds>& times) {
        std::sort(times.begin(), times.end());
        
        double avg = 0.0;
        for (const auto& t : times) {
            avg += t.count();
        }
        avg /= times.size();
        
        double p50 = times[times.size() / 2].count();
        double p95 = times[static_cast<size_t>(times.size() * 0.95)].count();
        double p99 = times[static_cast<size_t>(times.size() * 0.99)].count();
        
        return std::make_tuple(avg, p50, p95, p99);
    };
    
    auto [ins_avg, ins_p50, ins_p95, ins_p99] = calc_stats(insert_times);
    auto [find_avg, find_p50, find_p95, find_p99] = calc_stats(find_times);
    
    std::cout << "  Insert - Avg: " << static_cast<int>(ins_avg) << "ns, "
              << "P50: " << static_cast<int>(ins_p50) << "ns, "
              << "P95: " << static_cast<int>(ins_p95) << "ns, "
              << "P99: " << static_cast<int>(ins_p99) << "ns\n";
    std::cout << "  Find   - Avg: " << static_cast<int>(find_avg) << "ns, "
              << "P50: " << static_cast<int>(find_p50) << "ns, "
              << "P95: " << static_cast<int>(find_p95) << "ns, "
              << "P99: " << static_cast<int>(find_p99) << "ns\n\n";
}

void benchmark_read_heavy_workload() {
    std::cout << "=== Read-Heavy Workload (80% reads) ===\n\n";
    
    benchmark_tree_throughput<AtomicRBTree<int, int>>("Lock-free RBTree", 8, 10000, 80);
    benchmark_tree_throughput<MutexRBTree<int, int>>("Mutex RBTree", 8, 10000, 80);
}

void benchmark_write_heavy_workload() {
    std::cout << "=== Write-Heavy Workload (20% reads) ===\n\n";
    
    benchmark_tree_throughput<AtomicRBTree<int, int>>("Lock-free RBTree", 8, 10000, 20);
    benchmark_tree_throughput<MutexRBTree<int, int>>("Mutex RBTree", 8, 10000, 20);
}

void benchmark_balanced_workload() {
    std::cout << "=== Balanced Workload (50% reads) ===\n\n";
    
    benchmark_tree_throughput<AtomicRBTree<int, int>>("Lock-free RBTree", 8, 10000, 50);
    benchmark_tree_throughput<MutexRBTree<int, int>>("Mutex RBTree", 8, 10000, 50);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 5000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_tree_throughput<AtomicRBTree<int, int>>("Lock-free RBTree", threads, ops_per_thread, 60);
        benchmark_tree_throughput<MutexRBTree<int, int>>("Mutex RBTree", threads, ops_per_thread, 60);
    }
}

void benchmark_string_keys() {
    std::cout << "=== String Key Performance ===\n\n";
    
    // Pre-generate test data
    std::vector<std::string> test_keys;
    std::vector<int> test_values;
    std::vector<int> test_ops;
    
    constexpr int total_ops = 5000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> value_dist(1, 100000);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < total_ops; ++i) {
        test_keys.push_back("key_" + std::to_string(i) + "_suffix");
        test_values.push_back(value_dist(gen));
        test_ops.push_back(op_dist(gen));
    }
    
    auto string_benchmark = [&](auto& tree, const std::string& name) {
        constexpr int num_threads = 4;
        constexpr int operations_per_thread = 1250;
        std::atomic<bool> start_flag{false};
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                // Wait for start signal
                while (!start_flag.load(std::memory_order_acquire)) {
                    // Spin wait
                }
                
                std::uniform_int_distribution<> key_dist(0, test_keys.size() - 1);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    int idx = (t * operations_per_thread + i) % test_keys.size();
                    const std::string& key = test_keys[idx];
                    int op = test_ops[idx];
                    
                    if (op < 60) {
                        // Insert/update
                        tree.insert(key, test_values[idx]);
                    } else {
                        // Lookup
                        int value;
                        tree.find(key, value);
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
        
        std::cout << name << " string key performance:\n";
        std::cout << "  Time: " << duration.count() << " μs\n";
        std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
        std::cout << "  Tree size: " << tree.size() << "\n\n";
    };
    
    AtomicRBTree<std::string, int> lockfree_tree;
    MutexRBTree<std::string, int> mutex_tree;
    
    string_benchmark(lockfree_tree, "Lock-free RBTree");
    string_benchmark(mutex_tree, "Mutex RBTree");
}

int main() {
    std::cout << "RBTree Performance Benchmark\n";
    std::cout << "============================\n\n";
    
    benchmark_scaling();
    benchmark_read_heavy_workload();
    benchmark_write_heavy_workload();
    benchmark_balanced_workload();
    benchmark_string_keys();
    
    return 0;
}
