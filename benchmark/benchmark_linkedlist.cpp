#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <list>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_linkedlist.hpp"

using namespace lockfree;

// Mutex-based linked list for comparison
template<typename T>
class MutexLinkedList {
private:
    std::list<T> list_;
    mutable std::mutex mutex_;
    
public:
    void push_front(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.push_front(value);
    }
    
    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.push_back(value);
    }
    
    bool insert(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.push_front(value);  // Use push_front to match linked list behavior
        return true;
    }
    
    bool pop_front(T& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (list_.empty()) {
            return false;
        }
        result = list_.front();
        list_.pop_front();
        return true;
    }
    
    bool contains(const T& value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::find(list_.begin(), list_.end(), value) != list_.end();
    }
    
    bool remove(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(list_.begin(), list_.end(), value);
        if (it != list_.end()) {
            list_.erase(it);
            return true;
        }
        return false;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.size();
    }
};

template<typename ListType>
void benchmark_list_throughput(const std::string& name, int num_threads, 
                              int operations_per_thread, int read_percentage) {
    ListType list;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all random data to avoid RNG during timing
    constexpr int value_pool_size = 100000;
    std::vector<int> pre_generated_values(value_pool_size);
    std::vector<int> pre_generated_ops(value_pool_size);
    
    // Initialize outside timing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> value_dist(1, 10000);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < value_pool_size; ++i) {
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
            
            int value_index = t * 1000;  // Different starting point per thread
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int op = pre_generated_ops[(value_index + i) % value_pool_size];
                int value = pre_generated_values[(value_index + i) % value_pool_size];
                
                if (op < read_percentage) {
                    // Read operation (contains)
                    list.contains(value);
                } else if (op < read_percentage + 30) {
                    // Insert operation
                    list.insert(value);
                } else {
                    // Remove operation
                    list.remove(value);
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
    std::cout << "  Final list size: " << list.size() << "\n\n";
}

void benchmark_read_heavy_workload() {
    std::cout << "=== Read-Heavy Workload (80% reads) ===\n\n";
    
    benchmark_list_throughput<AtomicLinkedList<int>>("Lock-free LinkedList", 8, 10000, 80);
    benchmark_list_throughput<MutexLinkedList<int>>("Mutex LinkedList", 8, 10000, 80);
}

void benchmark_write_heavy_workload() {
    std::cout << "=== Write-Heavy Workload (20% reads) ===\n\n";
    
    benchmark_list_throughput<AtomicLinkedList<int>>("Lock-free LinkedList", 8, 10000, 20);
    benchmark_list_throughput<MutexLinkedList<int>>("Mutex LinkedList", 8, 10000, 20);
}

void benchmark_balanced_workload() {
    std::cout << "=== Balanced Workload (50% reads) ===\n\n";
    
    benchmark_list_throughput<AtomicLinkedList<int>>("Lock-free LinkedList", 8, 10000, 50);
    benchmark_list_throughput<MutexLinkedList<int>>("Mutex LinkedList", 8, 10000, 50);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 5000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_list_throughput<AtomicLinkedList<int>>("Lock-free LinkedList", threads, ops_per_thread, 60);
        benchmark_list_throughput<MutexLinkedList<int>>("Mutex LinkedList", threads, ops_per_thread, 60);
    }
}

int main() {
    std::cout << "LinkedList Performance Benchmark\n";
    std::cout << "================================\n\n";
    
    benchmark_scaling();
    benchmark_read_heavy_workload();
    benchmark_write_heavy_workload();
    benchmark_balanced_workload();
    
    return 0;
} 