#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <queue>
#include <atomic>
#include <limits>
#include <condition_variable>
#include "lockfree/atomic_mpmc_queue.hpp"

using namespace lockfree;

// Mutex-based queue for comparison
template<typename T>
class MutexQueue {
public:
    using value_type = T;

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    size_t max_size_;
    
public:
    explicit MutexQueue(size_t max_size = std::numeric_limits<size_t>::max()) 
        : max_size_(max_size) {}
    
    bool enqueue(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(item);
        return true;
    }
    
    bool enqueue(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(std::move(item));
        return true;
    }
    
    template<typename... Args>
    bool emplace(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            return false;
        }
        queue_.emplace(std::forward<Args>(args)...);
        return true;
    }
    
    bool dequeue(T& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        result = std::move(queue_.front());
        queue_.pop();
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
    
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size() >= max_size_;
    }
    
    constexpr size_t capacity() const {
        return max_size_;
    }
};

template<typename QueueType>
void benchmark_mpmc_throughput(const std::string& name, int num_producers, int num_consumers, 
                               int operations_per_producer) {
    QueueType queue;
    using value_type = typename QueueType::value_type;
    
    std::atomic<bool> start_flag{false};
    std::atomic<bool> producers_done{false};
    
    std::cout << name << " (" << num_producers << "P/" << num_consumers << "C):\n";
    
    std::vector<std::thread> threads;
    
    // Producer threads
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                // Spin wait
            }
            
            value_type item;
            if constexpr (std::is_same_v<value_type, int>) {
                item = p;
            } else {
                item = value_type{};
            }
            
            for (int i = 0; i < operations_per_producer; ++i) {
                while (!queue.enqueue(item)) {
                    // Spin until enqueue succeeds (handle bounded queue)
                }
                
                // Update item occasionally to avoid constant values
                if constexpr (std::is_same_v<value_type, int>) {
                    if (i % 1000 == 0) {
                        item = p * 10000 + i;
                    }
                }
            }
        });
    }
    
    // Consumer threads
    int operations_per_consumer = (num_producers * operations_per_producer) / num_consumers;
    for (int c = 0; c < num_consumers; ++c) {
        threads.emplace_back([&, c, operations_per_consumer]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                // Spin wait
            }
            
            value_type item;
            int consumed = 0;
            
            while (consumed < operations_per_consumer) {
                if (queue.dequeue(item)) {
                    consumed++;
                    
                    // Simulate minimal processing work
                    if constexpr (std::is_same_v<value_type, int>) {
                        volatile int dummy = item * 2; // Prevent optimization
                        (void)dummy;
                    }
                } else if (producers_done.load(std::memory_order_acquire)) {
                    // If producers are done and queue is empty, finish
                    break;
                }
            }
        });
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    
    // Wait for producers to finish
    for (int i = 0; i < num_producers; ++i) {
        threads[i].join();
    }
    producers_done.store(true, std::memory_order_release);
    
    // Wait for consumers to finish
    for (int i = num_producers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    int total_ops = num_producers * operations_per_producer;
    double throughput = (total_ops * 1000000.0) / duration.count();
    
    std::cout << "  Time: " << duration.count() << " μs\n";
    std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
    std::cout << "  Final queue size: " << queue.size() << "\n\n";
}

void benchmark_scaling_performance() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {2, 4, 8, 16};
    constexpr int ops_per_producer = 15000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        
        // Split threads evenly between producers and consumers
        int producers = threads / 2;
        int consumers = threads / 2;
        if (producers == 0) producers = 1;
        if (consumers == 0) consumers = 1;
        
        benchmark_mpmc_throughput<AtomicMPMCQueue<int, 1024>>(
            "Lock-free MPMC Queue", producers, consumers, ops_per_producer);
        
        benchmark_mpmc_throughput<MutexQueue<int>>(
            "Mutex Queue", producers, consumers, ops_per_producer);
    }
}

void benchmark_mixed_contention() {
    std::cout << "=== Mixed Contention Patterns ===\n\n";
    
    struct TestConfig {
        int producers;
        int consumers;
        std::string description;
        int ops_per_producer;
    };
    
    std::vector<TestConfig> configs = {
        {1, 7, "Producer Starved", 20000},
        {7, 1, "Consumer Starved", 5000},
        {4, 4, "Balanced High Contention", 10000},
        {8, 8, "Extreme Contention", 8000}
    };
    
    for (const auto& config : configs) {
        std::cout << "--- " << config.description << " (" 
                  << config.producers << "P/" << config.consumers << "C) ---\n";
        
        benchmark_mpmc_throughput<AtomicMPMCQueue<int, 2048>>(
            "Lock-free MPMC Queue", config.producers, config.consumers, config.ops_per_producer);
        
        benchmark_mpmc_throughput<MutexQueue<int>>(
            "Mutex Queue", config.producers, config.consumers, config.ops_per_producer);
    }
}

void benchmark_high_throughput() {
    std::cout << "=== High Throughput Test ===\n\n";
    
    constexpr int producers = 4;
    constexpr int consumers = 4;
    constexpr int ops_per_producer = 25000;
    
    benchmark_mpmc_throughput<AtomicMPMCQueue<int, 4096>>(
        "Lock-free MPMC Queue", producers, consumers, ops_per_producer);
    
    benchmark_mpmc_throughput<MutexQueue<int>>(
        "Mutex Queue", producers, consumers, ops_per_producer);
}

int main() {
    std::cout << "MPMC Queue Performance Benchmarks\n";
    std::cout << "==================================\n\n";
    
    benchmark_scaling_performance();
    benchmark_mixed_contention();
    benchmark_high_throughput();
    
    return 0;
} 