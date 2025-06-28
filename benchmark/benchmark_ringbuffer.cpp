#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_ringbuffer.hpp"

using namespace lockfree;

// Mutex-based bounded queue for fair SPSC comparison
template<typename T, size_t Capacity>
class MutexBoundedQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    const size_t capacity_;
    
public:
    MutexBoundedQueue() : capacity_(Capacity) {}
    
    bool push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false; // Non-blocking version
        }
        queue_.push(item);
        return true;
    }
    
    bool pop(T& result) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        result = queue_.front();
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
        return queue_.size() >= capacity_;
    }
    
    size_t capacity() const {
        return capacity_;
    }
};

template<typename BufferType>
double benchmark_spsc_throughput(const std::string& name, int num_items) {
    BufferType buffer;
    std::atomic<bool> start_flag{false};
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        while (!start_flag.load(std::memory_order_acquire)) {
            // Spin wait for start
        }
        
        for (int i = 0; i < num_items; ++i) {
            while (!buffer.push(i)) {
                // Busy wait - this is typical for SPSC scenarios
            }
        }
        producer_done.store(true, std::memory_order_release);
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!start_flag.load(std::memory_order_acquire)) {
            // Spin wait for start
        }
        
        int value;
        int consumed = 0;
        while (consumed < num_items) {
            if (buffer.pop(value)) {
                consumed++;
            }
        }
    });
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double throughput = (num_items * 1000000.0) / duration.count();
    
    std::cout << "  " << std::setw(25) << std::left << name << ": " 
              << std::setw(12) << std::right << static_cast<long>(throughput) 
              << " items/sec (" << duration.count() << " μs)\n";
    
    return throughput;
}

template<typename BufferType>
double benchmark_spsc_bursty(const std::string& name, int num_bursts, int burst_size, int burst_gap_us) {
    BufferType buffer;
    std::atomic<bool> start_flag{false};
    std::atomic<bool> producer_done{false};
    
    int total_items = num_bursts * burst_size;
    
    // Producer thread - sends in bursts
    std::thread producer([&]() {
        while (!start_flag.load(std::memory_order_acquire)) {
            // Spin wait for start
        }
        
        for (int burst = 0; burst < num_bursts; ++burst) {
            // Send burst
            for (int i = 0; i < burst_size; ++i) {
                int item = burst * burst_size + i;
                while (!buffer.push(item)) {
                    // Busy wait
                }
            }
            
            // Gap between bursts
            if (burst < num_bursts - 1) {
                std::this_thread::sleep_for(std::chrono::microseconds(burst_gap_us));
            }
        }
        producer_done.store(true, std::memory_order_release);
    });
    
    // Consumer thread - processes continuously
    std::thread consumer([&]() {
        while (!start_flag.load(std::memory_order_acquire)) {
            // Spin wait for start
        }
        
        int value;
        int consumed = 0;
        while (consumed < total_items) {
            if (buffer.pop(value)) {
                consumed++;
            }
        }
    });
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double throughput = (total_items * 1000000.0) / duration.count();
    
    std::cout << "  " << std::setw(25) << std::left << name << ": " 
              << std::setw(12) << std::right << static_cast<long>(throughput) 
              << " items/sec (" << duration.count() << " μs)\n";
    
    return throughput;
}

void benchmark_spsc_high_throughput() {
    std::cout << "=== SPSC High Throughput Test (1M items) ===\n";
    
    constexpr int num_items = 1000000;
    
    double lockfree_throughput = benchmark_spsc_throughput<AtomicRingBuffer<int, 4096>>("Lock-free RingBuffer", num_items);
    double mutex_throughput = benchmark_spsc_throughput<MutexBoundedQueue<int, 4096>>("Mutex BoundedQueue", num_items);
    
    double speedup = lockfree_throughput / mutex_throughput;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
}

void benchmark_spsc_different_sizes() {
    std::cout << "=== SPSC Buffer Size Impact (100K items) ===\n";
    
    constexpr int num_items = 100000;
    
    std::cout << "RingBuffer sizes:\n";
    double rb_256 = benchmark_spsc_throughput<AtomicRingBuffer<int, 256>>("RingBuffer-256", num_items);
    double rb_1024 = benchmark_spsc_throughput<AtomicRingBuffer<int, 1024>>("RingBuffer-1024", num_items);
    double rb_4096 = benchmark_spsc_throughput<AtomicRingBuffer<int, 4096>>("RingBuffer-4096", num_items);
    
    std::cout << "\nMutex BoundedQueue sizes:\n";
    double mutex_256 = benchmark_spsc_throughput<MutexBoundedQueue<int, 256>>("MutexQueue-256", num_items);
    double mutex_1024 = benchmark_spsc_throughput<MutexBoundedQueue<int, 1024>>("MutexQueue-1024", num_items);
    double mutex_4096 = benchmark_spsc_throughput<MutexBoundedQueue<int, 4096>>("MutexQueue-4096", num_items);
    
    std::cout << "\nSpeedups:\n";
    std::cout << "  256 elements:  " << std::fixed << std::setprecision(2) << (rb_256 / mutex_256) << "x\n";
    std::cout << "  1024 elements: " << std::fixed << std::setprecision(2) << (rb_1024 / mutex_1024) << "x\n";
    std::cout << "  4096 elements: " << std::fixed << std::setprecision(2) << (rb_4096 / mutex_4096) << "x\n\n";
}

void benchmark_spsc_bursty_traffic() {
    std::cout << "=== SPSC Bursty Traffic Test ===\n";
    
    constexpr int num_bursts = 100;
    constexpr int burst_size = 500;
    constexpr int burst_gap_us = 100; // 100 microsecond gaps
    
    std::cout << "Traffic pattern: " << num_bursts << " bursts of " << burst_size 
              << " items with " << burst_gap_us << "μs gaps\n";
    
    double lockfree_throughput = benchmark_spsc_bursty<AtomicRingBuffer<int, 1024>>("Lock-free RingBuffer", num_bursts, burst_size, burst_gap_us);
    double mutex_throughput = benchmark_spsc_bursty<MutexBoundedQueue<int, 1024>>("Mutex BoundedQueue", num_bursts, burst_size, burst_gap_us);
    
    double speedup = lockfree_throughput / mutex_throughput;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
}

void benchmark_spsc_latency() {
    std::cout << "=== SPSC Latency Test (Single item round-trips) ===\n";
    
    constexpr int num_iterations = 10000;
    
    // RingBuffer latency
    {
        AtomicRingBuffer<int, 1024> buffer;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_iterations; ++i) {
            while (!buffer.push(i)) {}
            int value;
            while (!buffer.pop(value)) {}
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        
        double avg_latency = static_cast<double>(duration.count()) / (num_iterations * 2); // push + pop
        std::cout << "  Lock-free RingBuffer  : " << std::fixed << std::setprecision(1) << avg_latency << " ns/operation\n";
    }
    
    // Mutex queue latency
    {
        MutexBoundedQueue<int, 1024> queue;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_iterations; ++i) {
            while (!queue.push(i)) {}
            int value;
            while (!queue.pop(value)) {}
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        
        double avg_latency = static_cast<double>(duration.count()) / (num_iterations * 2); // push + pop
        std::cout << "  Mutex BoundedQueue    : " << std::fixed << std::setprecision(1) << avg_latency << " ns/operation\n";
    }
    
    std::cout << "\n";
}

void benchmark_standard_comparison() {
    std::cout << "=== Standard Benchmark (for performance table) ===\n";
    std::cout << "Using realistic bursty traffic pattern (200 bursts of 250 items, 50μs gaps)\n";
    
    constexpr int num_bursts = 200;
    constexpr int burst_size = 250;
    constexpr int burst_gap_us = 50; // 50 microsecond gaps - realistic for many scenarios
    
    double lockfree_throughput = benchmark_spsc_bursty<AtomicRingBuffer<int, 1024>>("Lock-free RingBuffer", num_bursts, burst_size, burst_gap_us);
    double mutex_throughput = benchmark_spsc_bursty<MutexBoundedQueue<int, 1024>>("Mutex BoundedQueue", num_bursts, burst_size, burst_gap_us);
    
    double speedup = lockfree_throughput / mutex_throughput;
    std::cout << "  Throughput: " << static_cast<long>(lockfree_throughput) << " items/sec\n";
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
}

int main() {
    std::cout << "RingBuffer SPSC Performance Benchmark\n";
    std::cout << "=====================================\n";
    std::cout << "RingBuffer is designed for Single Producer Single Consumer scenarios\n";
    std::cout << "This benchmark tests its intended use case where it should excel.\n\n";
    
    benchmark_standard_comparison();
    benchmark_spsc_high_throughput();
    benchmark_spsc_different_sizes();
    benchmark_spsc_bursty_traffic();
    benchmark_spsc_latency();
    
    std::cout << "Note: RingBuffer should NOT be used in MPMC scenarios.\n";
    std::cout << "For multi-producer/consumer use cases, consider AtomicMPMCQueue instead.\n";
    
    return 0;
}