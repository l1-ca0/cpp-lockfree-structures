#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <array>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_bloomfilter.hpp"

using namespace lockfree;

// Simple mutex-based Bloom filter for comparison
template<typename T, size_t Size = 8192, size_t NumHashFunctions = 3>
class MutexBloomFilter {
private:
    std::array<uint64_t, Size / 64> bits_;
    size_t approximate_count_;
    mutable std::mutex mutex_;
    std::hash<T> hasher_;
    
    static constexpr std::array<uint64_t, 3> hash_seeds_ = {
        0x9e3779b9, 0x85ebca6b, 0xc2b2ae35
    };
    
public:
    MutexBloomFilter() : approximate_count_(0) {
        std::fill(bits_.begin(), bits_.end(), 0);
    }
    
    bool insert(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t base_hash = hasher_(item);
        bool was_new = false;
        
        for (size_t i = 0; i < NumHashFunctions; ++i) {
            size_t hash_val = (base_hash ^ hash_seeds_[i % 3]) & (Size - 1);
            size_t word_index = hash_val / 64;
            size_t bit_offset = hash_val % 64;
            uint64_t bit_mask = 1ULL << bit_offset;
            
            if ((bits_[word_index] & bit_mask) == 0) {
                bits_[word_index] |= bit_mask;
                was_new = true;
            }
        }
        
        if (was_new) {
            approximate_count_++;
        }
        return was_new;
    }
    
    bool contains(const T& item) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t base_hash = hasher_(item);
        
        for (size_t i = 0; i < NumHashFunctions; ++i) {
            size_t hash_val = (base_hash ^ hash_seeds_[i % 3]) & (Size - 1);
            size_t word_index = hash_val / 64;
            size_t bit_offset = hash_val % 64;
            uint64_t bit_mask = 1ULL << bit_offset;
            
            if ((bits_[word_index] & bit_mask) == 0) {
                return false;
            }
        }
        return true;
    }
    
    size_t approximate_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return approximate_count_;
    }
    
    double load_factor() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (uint64_t word : bits_) {
            count += __builtin_popcountll(word);
        }
        return static_cast<double>(count) / Size;
    }
};

struct Operation {
    int value;
    bool is_insert;  // true = insert, false = contains
    
    Operation(int v = 0, bool insert = true) : value(v), is_insert(insert) {}
};

template<typename FilterType>
void benchmark_throughput(const std::string& name, int num_threads, int operations_per_thread) {
    FilterType filter;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all operations to avoid random generation during timing
    std::vector<std::vector<Operation>> thread_ops(num_threads);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> value_dist(1, 100000);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int t = 0; t < num_threads; ++t) {
        thread_ops[t].reserve(operations_per_thread);
        for (int i = 0; i < operations_per_thread; ++i) {
            int value = value_dist(gen);
            bool is_insert = op_dist(gen) < 50; // 50% insert, 50% contains
            thread_ops[t].emplace_back(value, is_insert);
        }
    }
    
    std::cout << name << " (" << num_threads << " threads):\n";
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                // Spin wait
            }
            
            for (const auto& op : thread_ops[t]) {
                if (op.is_insert) {
                    filter.insert(op.value);
                } else {
                    volatile bool result = filter.contains(op.value);
                    (void)result; // Suppress unused variable warning
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
    
    int total_ops = num_threads * operations_per_thread;
    double throughput = (total_ops * 1000000.0) / duration.count();
    
    std::cout << "  Time: " << duration.count() << " μs\n";
    std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
    std::cout << "  Size: " << filter.approximate_size() << "\n";
    std::cout << "  Load factor: " << std::fixed << std::setprecision(4) << filter.load_factor() << "\n\n";
}

void benchmark_insert_heavy() {
    std::cout << "=== Insert Heavy Workload ===\n\n";
    
    constexpr int threads = 8;
    constexpr int ops = 15000;
    
    benchmark_throughput<AtomicBloomFilter<int, 16384, 3>>("Lock-free BloomFilter", threads, ops);
    benchmark_throughput<MutexBloomFilter<int, 16384, 3>>("Mutex BloomFilter", threads, ops);
}

void benchmark_lookup_heavy() {
    std::cout << "=== Lookup Heavy Workload ===\n\n";
    
    // Modify benchmark for lookup-heavy (80% lookup, 20% insert)
    auto lookup_heavy_benchmark = [](const std::string& name, int num_threads, int operations_per_thread) {
        AtomicBloomFilter<int, 16384, 3> filter;
        std::atomic<bool> start_flag{false};
        
        std::vector<std::vector<Operation>> thread_ops(num_threads);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> value_dist(1, 100000);
        std::uniform_int_distribution<> op_dist(0, 99);
        
        for (int t = 0; t < num_threads; ++t) {
            thread_ops[t].reserve(operations_per_thread);
            for (int i = 0; i < operations_per_thread; ++i) {
                int value = value_dist(gen);
                bool is_insert = op_dist(gen) < 20; // 20% insert, 80% contains
                thread_ops[t].emplace_back(value, is_insert);
            }
        }
        
        std::cout << name << " (" << num_threads << " threads):\n";
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                while (!start_flag.load(std::memory_order_acquire)) {
                    // Spin wait
                }
                
                for (const auto& op : thread_ops[t]) {
                    if (op.is_insert) {
                        filter.insert(op.value);
                    } else {
                        volatile bool result = filter.contains(op.value);
                        (void)result;
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
        
        int total_ops = num_threads * operations_per_thread;
        double throughput = (total_ops * 1000000.0) / duration.count();
        
        std::cout << "  Time: " << duration.count() << " μs\n";
        std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n\n";
    };
    
    constexpr int threads = 8;
    constexpr int ops = 15000;
    
    lookup_heavy_benchmark("Lock-free BloomFilter", threads, ops);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 10000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_throughput<AtomicBloomFilter<int, 16384, 3>>("Lock-free BloomFilter", threads, ops_per_thread);
        benchmark_throughput<MutexBloomFilter<int, 16384, 3>>("Mutex BloomFilter", threads, ops_per_thread);
    }
}

int main() {
    std::cout << "BloomFilter Performance Benchmarks\n";
    std::cout << "===================================\n\n";
    
    benchmark_scaling();
    benchmark_insert_heavy();
    benchmark_lookup_heavy();
    
    return 0;
} 