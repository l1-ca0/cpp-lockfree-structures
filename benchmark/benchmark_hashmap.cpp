#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <random>
#include <algorithm>
#include "lockfree/atomic_hashmap.hpp"

using namespace lockfree;

// Mutex-based hash map for comparison
template<typename Key, typename Value>
class MutexHashMap {
private:
    std::unordered_map<Key, Value> map_;
    mutable std::mutex mutex_;
    
public:
    bool insert(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result = map_.insert({key, value});
        return result.second;
    }
    
    bool find(const Key& key, Value& result) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            result = it->second;
            return true;
        }
        return false;
    }
    
    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.erase(key) > 0;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.empty();
    }
    
    double load_factor() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.load_factor();
    }
};

template<typename MapType>
void benchmark_map_throughput(const std::string& name, int num_threads, 
                             int operations_per_thread, int read_percentage) {
    MapType map;
    std::atomic<bool> start_flag{false};
    
    // Pre-generate all random data to avoid RNG during timing
    constexpr int key_pool_size = 100000;
    std::vector<int> pre_generated_keys(key_pool_size);
    std::vector<int> pre_generated_ops(key_pool_size);
    std::vector<int> pre_generated_values(key_pool_size);
    
    // Initialize outside timing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(1, operations_per_thread * num_threads / 2);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    for (int i = 0; i < key_pool_size; ++i) {
        pre_generated_keys[i] = key_dist(gen);
        pre_generated_ops[i] = op_dist(gen);
        pre_generated_values[i] = i;
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
                int key = pre_generated_keys[(key_index + i) % key_pool_size];
                int op = pre_generated_ops[(key_index + i) % key_pool_size];
                int value = pre_generated_values[(key_index + i) % key_pool_size];
                
                if (op < read_percentage) {
                    // Read operation
                    int result;
                    map.find(key, result);
                } else if (op < read_percentage + 25) {
                    // Insert operation
                    map.insert(key, value);
                } else {
                    // Erase operation
                    map.erase(key);
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
    std::cout << "  Final map size: " << map.size() << "\n\n";
}

template<typename MapType>
void benchmark_map_latency(const std::string& name, int num_operations) {
    MapType map;
    std::vector<std::chrono::nanoseconds> insert_times;
    std::vector<std::chrono::nanoseconds> find_times;
    
    insert_times.reserve(num_operations);
    find_times.reserve(num_operations);
    
    std::cout << "Latency benchmark for " << name << " (" << num_operations << " operations)\n";
    
    // Pre-populate for find operations
    for (int i = 0; i < num_operations / 2; ++i) {
        map.insert(i, i * 10);
    }
    
    // Measure insert latency
    for (int i = num_operations / 2; i < num_operations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        map.insert(i, i * 10);
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
        map.find(key, value);
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
    
    benchmark_map_throughput<AtomicHashMap<int, int>>("Lock-free HashMap", 8, 10000, 80);
    benchmark_map_throughput<MutexHashMap<int, int>>("Mutex HashMap", 8, 10000, 80);
}

void benchmark_write_heavy_workload() {
    std::cout << "=== Write-Heavy Workload (20% reads) ===\n\n";
    
    benchmark_map_throughput<AtomicHashMap<int, int>>("Lock-free HashMap", 8, 10000, 20);
    benchmark_map_throughput<MutexHashMap<int, int>>("Mutex HashMap", 8, 10000, 20);
}

void benchmark_balanced_workload() {
    std::cout << "=== Balanced Workload (50% reads) ===\n\n";
    
    benchmark_map_throughput<AtomicHashMap<int, int>>("Lock-free HashMap", 8, 10000, 50);
    benchmark_map_throughput<MutexHashMap<int, int>>("Mutex HashMap", 8, 10000, 50);
}

void benchmark_scaling() {
    std::cout << "=== Scaling Benchmark ===\n\n";
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    constexpr int ops_per_thread = 5000;
    
    for (int threads : thread_counts) {
        std::cout << "--- " << threads << " threads ---\n";
        benchmark_map_throughput<AtomicHashMap<int, int>>("Lock-free HashMap", threads, ops_per_thread, 60);
        benchmark_map_throughput<MutexHashMap<int, int>>("Mutex HashMap", threads, ops_per_thread, 60);
    }
}

void benchmark_cache_simulation() {
    std::cout << "=== Cache Access Pattern Simulation ===\n\n";
    
    constexpr int cache_size = 1000;
    constexpr int working_set = 100;  // 80-20 rule simulation
    
    AtomicHashMap<int, std::string> lockfree_cache;
    MutexHashMap<int, std::string> mutex_cache;
    
    // Pre-populate caches
    for (int i = 0; i < cache_size; ++i) {
        std::string value = "cached_data_" + std::to_string(i);
        lockfree_cache.insert(i, value);
        mutex_cache.insert(i, value);
    }
    
    auto cache_benchmark = [](auto& cache, const std::string& name) {
        constexpr int num_threads = 6;
        constexpr int operations_per_thread = 10000;
        
        std::atomic<int> hits{0};
        std::atomic<int> misses{0};
        
        std::vector<std::thread> threads;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> key_dist(0, cache_size * 2);
                std::uniform_int_distribution<> locality_dist(0, 99);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    int key;
                    if (locality_dist(gen) < 80) {
                        // 80% access to hot data (working set)
                        key = key_dist(gen) % working_set;
                    } else {
                        // 20% access to cold data
                        key = working_set + (key_dist(gen) % (cache_size - working_set));
                    }
                    
                    std::string value;
                    if (cache.find(key, value)) {
                        hits.fetch_add(1);
                    } else {
                        misses.fetch_add(1);
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int total_operations = num_threads * operations_per_thread;
        double throughput = (total_operations * 1000.0) / duration.count();
        double hit_rate = (100.0 * hits.load()) / (hits.load() + misses.load());
        
        std::cout << name << " cache simulation:\n";
        std::cout << "  Time: " << duration.count() << " ms\n";
        std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
        std::cout << "  Cache hits: " << hits.load() << "\n";
        std::cout << "  Cache misses: " << misses.load() << "\n";
        std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) << hit_rate << "%\n\n";
    };
    
    cache_benchmark(lockfree_cache, "Lock-free HashMap");
    cache_benchmark(mutex_cache, "Mutex HashMap");
}

void benchmark_string_keys() {
    std::cout << "=== String Key Performance ===\n\n";
    
    AtomicHashMap<std::string, int> lockfree_map;
    MutexHashMap<std::string, int> mutex_map;
    
    // Generate test strings
    std::vector<std::string> test_keys;
    for (int i = 0; i < 10000; ++i) {
        test_keys.push_back("key_" + std::to_string(i) + "_suffix");
    }
    
    auto string_benchmark = [&](auto& map, const std::string& name) {
        constexpr int num_threads = 4;
        constexpr int operations_per_thread = 2500;
        
        std::atomic<int> operations_completed{0};
        
        std::vector<std::thread> threads;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> key_dist(0, test_keys.size() - 1);
                std::uniform_int_distribution<> op_dist(0, 99);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    const std::string& key = test_keys[key_dist(gen)];
                    
                    if (op_dist(gen) < 60) {
                        // Insert/update
                        map.insert(key, t * operations_per_thread + i);
                    } else {
                        // Lookup
                        int value;
                        map.find(key, value);
                    }
                    
                    operations_completed.fetch_add(1);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        double throughput = (operations_completed.load() * 1000.0) / duration.count();
        
        std::cout << name << " string key performance:\n";
        std::cout << "  Time: " << duration.count() << " ms\n";
        std::cout << "  Throughput: " << static_cast<long>(throughput) << " ops/sec\n";
        std::cout << "  Map size: " << map.size() << "\n";
        std::cout << "  Load factor: " << map.load_factor() << "\n\n";
    };
    
    string_benchmark(lockfree_map, "Lock-free HashMap");
    string_benchmark(mutex_map, "Mutex HashMap");
}

int main() {
    std::cout << "HashMap Performance Benchmark\n";
    std::cout << "============================\n\n";
    
    benchmark_scaling();
    benchmark_read_heavy_workload();
    benchmark_write_heavy_workload();
    benchmark_balanced_workload();
    
    return 0;
}