#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <queue>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_priority_queue.hpp"

using namespace lockfree;

// Mutex-based priority queue for comparison
template<typename T, typename Compare = std::less<T>>
class MutexPriorityQueue {
private:
    std::priority_queue<T, std::vector<T>, Compare> queue_;
    mutable std::mutex mutex_;
    
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
    }
    
    bool pop(T& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        result = queue_.top();
        queue_.pop();
        return true;
    }
    
    bool top(T& result) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        result = queue_.top();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

template<typename QueueType>
void benchmark_priority_queue_throughput(const std::string& name, int num_threads, 
                                        int operations_per_thread, int read_percentage) {
    QueueType pq;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all random data to avoid RNG during timing
    constexpr int priority_pool_size = 100000;
    std::vector<int> pre_generated_priorities(priority_pool_size);
    std::vector<int> pre_generated_ops(priority_pool_size);
    
    // Initialize outside timing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> priority_dist(1, 1000);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < priority_pool_size; ++i) {
        pre_generated_priorities[i] = priority_dist(gen);
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
            
            int priority_index = t * 1000;  // Different starting point per thread
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int op = pre_generated_ops[(priority_index + i) % priority_pool_size];
                
                if (op < read_percentage / 2) {
                    // Top operation
                    int result;
                    pq.top(result);
                } else if (op < read_percentage) {
                    // Pop operation
                    int result;
                    pq.pop(result);
                } else {
                    // Push operation
                    int priority = pre_generated_priorities[(priority_index + i) % priority_pool_size];
                    pq.push(priority);
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
    std::cout << "  Final queue size: " << pq.size() << "\n\n";
}

void benchmark_read_heavy_workload() {
    std::cout << "=== Read-Heavy Workload (80% reads) ===\n\n";
    
    benchmark_priority_queue_throughput<AtomicPriorityQueue<int>>("Lock-free PriorityQueue", 8, 10000, 80);
    benchmark_priority_queue_throughput<MutexPriorityQueue<int>>("Mutex PriorityQueue", 8, 10000, 80);
}

void benchmark_write_heavy_workload() {
    std::cout << "=== Write-Heavy Workload (20% reads) ===\n\n";
    
    benchmark_priority_queue_throughput<AtomicPriorityQueue<int>>("Lock-free PriorityQueue", 8, 10000, 20);
    benchmark_priority_queue_throughput<MutexPriorityQueue<int>>("Mutex PriorityQueue", 8, 10000, 20);
}

void benchmark_balanced_workload() {
    std::cout << "=== Balanced Workload (50% reads) ===\n\n";
    
    benchmark_priority_queue_throughput<AtomicPriorityQueue<int>>("Lock-free PriorityQueue", 8, 10000, 50);
    benchmark_priority_queue_throughput<MutexPriorityQueue<int>>("Mutex PriorityQueue", 8, 10000, 50);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 5000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_priority_queue_throughput<AtomicPriorityQueue<int>>("Lock-free PriorityQueue", threads, ops_per_thread, 60);
        benchmark_priority_queue_throughput<MutexPriorityQueue<int>>("Mutex PriorityQueue", threads, ops_per_thread, 60);
    }
}

int main() {
    std::cout << "Priority Queue Performance Benchmark\n";
    std::cout << "====================================\n\n";
    
    benchmark_scaling();
    benchmark_read_heavy_workload();
    benchmark_write_heavy_workload();
    benchmark_balanced_workload();
    
    return 0;
} 