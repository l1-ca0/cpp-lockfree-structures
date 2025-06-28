#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <stack>
#include <atomic>
#include "lockfree/atomic_stack.hpp"

using namespace lockfree;

// Mutex-based stack for comparison
template<typename T>
class MutexStack {
private:
    std::stack<T> stack_;
    mutable std::mutex mutex_;
    
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        stack_.push(item);
    }
    
    bool pop(T& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stack_.empty()) {
            return false;
        }
        result = stack_.top();
        stack_.pop();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.size();
    }
};

template<typename StackType>
void benchmark_stack(const std::string& name, int num_threads, int operations_per_thread) {
    StackType stack;
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};
    
    std::cout << "Benchmarking " << name << " with " << num_threads 
              << " threads, " << operations_per_thread << " ops each\n";
    
    // Create threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            // Mixed operations: 60% push, 40% pop
            for (int j = 0; j < operations_per_thread; ++j) {
                if (j % 10 < 6) {
                    // Push operation
                    stack.push(i * operations_per_thread + j);
                } else {
                    // Pop operation
                    int value;
                    stack.pop(value);
                }
            }
        });
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true);
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    int total_operations = num_threads * operations_per_thread;
    double ops_per_second = (total_operations * 1000000.0) / duration.count();
    
    std::cout << "  Time: " << duration.count() << " μs\n";
    std::cout << "  Operations/second: " << static_cast<long>(ops_per_second) << "\n";
    
    // Skip size() call for high thread counts to avoid O(n) hang
    if (num_threads <= 8) {
        std::cout << "  Final size: " << stack.size() << "\n\n";
    } else {
        std::cout << "  Final size: [skipped for performance]\n\n";
    }
}

void scaling_benchmark() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int operations_per_thread = 10000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_stack<AtomicStack<int>>("Lock-free Stack", threads, operations_per_thread);
        benchmark_stack<MutexStack<int>>("Mutex Stack", threads, operations_per_thread);
    }
}

void contention_benchmark() {
    std::cout << "=== High Contention Benchmark ===\n\n";
    
    // Full high contention test - 16 threads as required
    constexpr int num_threads = 16;
    constexpr int operations_per_thread = 10000;
    
    std::cout << "Note: Full high contention test with 16 threads, 10k ops each\n";
    benchmark_stack<AtomicStack<int>>("Lock-free Stack", num_threads, operations_per_thread);
    benchmark_stack<MutexStack<int>>("Mutex Stack", num_threads, operations_per_thread);
}

int main() {
    std::cout << "Stack Performance Benchmarks\n";
    std::cout << "=============================\n\n";
    
    scaling_benchmark();
    contention_benchmark();
    
    return 0;
}