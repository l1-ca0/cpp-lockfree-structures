#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <deque>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_work_stealing_deque.hpp"

using namespace lockfree;

/**
 * Comprehensive benchmark suite for AtomicWorkStealingDeque
 */

// Simple task structure
struct Task {
    int id;
    int work_units;
    
    Task(int i = 0, int w = 0) : id(i), work_units(w) {}
};

// Mutex-based work deque for comparison
template<typename T>
class MutexWorkDeque {
private:
    std::deque<T> deque_;
    mutable std::mutex mutex_;
    
public:
    void push_bottom(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        deque_.push_back(item);
    }
    
    T* pop_bottom() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deque_.empty()) {
            return nullptr;
        }
        static thread_local T result;
        result = std::move(deque_.back());
        deque_.pop_back();
        return &result;
    }
    
    T* steal() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deque_.empty()) {
            return nullptr;
        }
        static thread_local T result;
        result = std::move(deque_.front());
        deque_.pop_front();
        return &result;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deque_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deque_.size();
    }
    
    static constexpr size_t capacity() {
        return 4095;
    }
};

template<typename DequeType>
void benchmark_work_stealing_throughput(const std::string& name, int num_worker_threads, 
                                       int operations_per_thread) {
    DequeType deque;
    std::atomic<bool> start_flag{false};
    std::atomic<bool> production_done{false};
    
    // Pre-generate all task data to avoid allocations during timing
    std::vector<Task> pre_generated_tasks(operations_per_thread);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> work_dist(1, 100);
    
    for (int i = 0; i < operations_per_thread; ++i) {
        pre_generated_tasks[i] = Task(i, work_dist(gen));
    }
    
    std::cout << "Benchmarking " << name << " - 1 owner + " << num_worker_threads 
              << " workers, " << operations_per_thread << " ops each\n";
    
    std::vector<std::thread> threads;
    
    // Owner thread - pushes tasks and occasionally pops own work
    threads.emplace_back([&]() {
        // Wait for start signal
        while (!start_flag.load(std::memory_order_acquire)) {
            // Spin wait
        }
        
        for (int i = 0; i < operations_per_thread; ++i) {
            // Push work
            deque.push_bottom(pre_generated_tasks[i]);
            
            // Occasionally process own work (20% of the time)
            if (i % 5 == 0) {
                auto* item = deque.pop_bottom();
                // No need to do anything with item, just testing performance
                (void)item;
            }
        }
        
        // Process remaining own work
        while (auto* item = deque.pop_bottom()) {
            (void)item; // Suppress unused variable warning
        }
    });
    
    // Worker threads - steal work from the deque
    for (int w = 0; w < num_worker_threads; ++w) {
        threads.emplace_back([&]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                // Spin wait
            }
            
            int work_done = 0;
            int max_work = operations_per_thread / num_worker_threads;
            
            while (work_done < max_work && !production_done.load()) {
                auto* item = deque.steal();
                if (item) {
                    work_done++;
                    // No processing needed, just testing steal performance
                }
                // No yield - pure CPU bound
            }
        });
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    
    // Wait for owner
    threads[0].join();
    production_done.store(true);
    
    // Wait for workers
    for (int i = 1; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double throughput = (operations_per_thread * 1000000.0) / duration.count();
    
    std::cout << "  Time: " << duration.count() << " μs\n";
    std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
    std::cout << "  Final deque size: " << deque.size() << "\n\n";
}

void benchmark_single_owner_multiple_thieves() {
    std::cout << "=== Single Owner Multiple Thieves ===\n\n";
    
    benchmark_work_stealing_throughput<AtomicWorkStealingDeque<Task>>("Lock-free WorkDeque", 4, 20000);
    benchmark_work_stealing_throughput<MutexWorkDeque<Task>>("Mutex WorkDeque", 4, 20000);
}

void benchmark_light_contention() {
    std::cout << "=== Light Contention (2 workers) ===\n\n";
    
    benchmark_work_stealing_throughput<AtomicWorkStealingDeque<Task>>("Lock-free WorkDeque", 2, 15000);
    benchmark_work_stealing_throughput<MutexWorkDeque<Task>>("Mutex WorkDeque", 2, 15000);
}

void benchmark_heavy_contention() {
    std::cout << "=== Heavy Contention (8 workers) ===\n\n";
    
    benchmark_work_stealing_throughput<AtomicWorkStealingDeque<Task>>("Lock-free WorkDeque", 8, 25000);
    benchmark_work_stealing_throughput<MutexWorkDeque<Task>>("Mutex WorkDeque", 8, 25000);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> worker_counts = {1, 2, 4, 6, 8, 16};
    constexpr int ops_per_test = 10000;
    
    for (int workers : worker_counts) {
        std::cout << "--- " << workers << " workers ---\n";
        benchmark_work_stealing_throughput<AtomicWorkStealingDeque<Task>>("Lock-free WorkDeque", workers, ops_per_test);
        benchmark_work_stealing_throughput<MutexWorkDeque<Task>>("Mutex WorkDeque", workers, ops_per_test);
    }
}

int main() {
    std::cout << "Work Stealing Deque Performance Benchmark\n";
    std::cout << "=========================================\n\n";
    
    benchmark_scaling();
    benchmark_single_owner_multiple_thieves();
    benchmark_light_contention();
    benchmark_heavy_contention();
    
    return 0;
} 