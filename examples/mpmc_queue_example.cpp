#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include "lockfree/atomic_mpmc_queue.hpp"

using namespace lockfree;

void producer_consumer_example() {
    std::cout << "=== MPMC Queue Producer-Consumer Example ===\n\n";
    
    // Create an MPMC queue with capacity of 1024 elements
    AtomicMPMCQueue<int, 1024> queue;
    
    constexpr int num_producers = 4;
    constexpr int num_consumers = 3;
    constexpr int items_per_producer = 1000;
    
    std::atomic<bool> producers_done{false};
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};
    std::vector<std::atomic<int>> producer_counts(num_producers);
    std::vector<std::atomic<int>> consumer_counts(num_consumers);
    
    // Initialize counters
    for (int i = 0; i < num_producers; ++i) {
        producer_counts[i] = 0;
    }
    for (int i = 0; i < num_consumers; ++i) {
        consumer_counts[i] = 0;
    }
    
    std::vector<std::thread> threads;
    
    // Create producer threads
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> value_dist(1000 * p, 1000 * p + 999);
            
            for (int i = 0; i < items_per_producer; ++i) {
                int value = value_dist(gen);
                
                // Try to enqueue with retry
                while (!queue.enqueue(value)) {
                    std::this_thread::yield();
                }
                
                producer_counts[p].fetch_add(1);
                total_produced.fetch_add(1);
                
                // Small delay to demonstrate concurrent operation
                if (i % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
            
            std::cout << "Producer " << p << " finished producing " 
                      << producer_counts[p].load() << " items\n";
        });
    }
    
    // Create consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        threads.emplace_back([&, c]() {
            int value;
            
            while (!producers_done.load() || !queue.empty()) {
                if (queue.dequeue(value)) {
                    consumer_counts[c].fetch_add(1);
                    total_consumed.fetch_add(1);
                    
                    // Verify the value is from expected range
                    int producer_id = value / 1000;
                    if (producer_id >= num_producers) {
                        std::cout << "ERROR: Invalid value " << value 
                                  << " from producer " << producer_id << std::endl;
                    }
                } else {
                    std::this_thread::yield();
                }
            }
            
            std::cout << "Consumer " << c << " finished consuming " 
                      << consumer_counts[c].load() << " items\n";
        });
    }
    
    // Wait for all producers to finish
    for (int i = 0; i < num_producers; ++i) {
        threads[i].join();
    }
    producers_done.store(true);
    
    // Wait for all consumers to finish
    for (int i = num_producers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    std::cout << "\nResults:\n";
    std::cout << "Total produced: " << total_produced.load() << "\n";
    std::cout << "Total consumed: " << total_consumed.load() << "\n";
    std::cout << "Items remaining in queue: " << queue.size() << "\n";
    std::cout << "Queue capacity: " << queue.capacity() << "\n";
    
    if (total_produced.load() == total_consumed.load()) {
        std::cout << "✅ All items successfully processed!\n";
    } else {
        std::cout << "❌ Mismatch in production/consumption counts!\n";
    }
}

void high_frequency_example() {
    std::cout << "\n=== High-Frequency Operation Example ===\n\n";
    
    AtomicMPMCQueue<std::string, 512> queue;
    
    constexpr int num_threads = 8;  // 4 producers, 4 consumers
    constexpr int operations_per_thread = 10000;
    
    std::atomic<int> enqueue_count{0};
    std::atomic<int> dequeue_count{0};
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Create producer threads
    for (int t = 0; t < num_threads / 2; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                std::string item = "thread" + std::to_string(t) + "_item" + std::to_string(i);
                
                while (!queue.enqueue(std::move(item))) {
                    // Busy wait with minimal overhead
                }
                enqueue_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Create consumer threads
    for (int t = num_threads / 2; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::string item;
            int local_count = 0;
            
            while (local_count < operations_per_thread) {
                if (queue.dequeue(item)) {
                    local_count++;
                    dequeue_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    int total_operations = enqueue_count.load() + dequeue_count.load();
    double ops_per_second = (total_operations * 1000000.0) / duration.count();
    
    std::cout << "High-frequency test results:\n";
    std::cout << "Total enqueues: " << enqueue_count.load() << "\n";
    std::cout << "Total dequeues: " << dequeue_count.load() << "\n";
    std::cout << "Total operations: " << total_operations << "\n";
    std::cout << "Time taken: " << duration.count() << " μs\n";
    std::cout << "Operations per second: " << static_cast<long>(ops_per_second) << "\n";
    std::cout << "Final queue size: " << queue.size() << "\n";
}

void capacity_and_bounds_example() {
    std::cout << "\n=== Capacity and Bounds Example ===\n\n";
    
    // Small queue to demonstrate capacity limits
    AtomicMPMCQueue<int, 8> small_queue;
    
    std::cout << "Queue capacity: " << small_queue.capacity() << "\n";
    std::cout << "Initial size: " << small_queue.size() << "\n";
    std::cout << "Is empty: " << (small_queue.empty() ? "true" : "false") << "\n";
    std::cout << "Is full: " << (small_queue.full() ? "true" : "false") << "\n\n";
    
    // Fill the queue
    std::cout << "Filling queue to capacity...\n";
    for (int i = 0; i < small_queue.capacity(); ++i) {
        bool success = small_queue.enqueue(i * 10);
        std::cout << "Enqueue " << (i * 10) << ": " << (success ? "success" : "failed") 
                  << " (size: " << small_queue.size() << ")\n";
    }
    
    std::cout << "\nQueue is now full: " << (small_queue.full() ? "true" : "false") << "\n";
    
    // Try to add one more (should fail)
    std::cout << "\nTrying to add to full queue...\n";
    bool overflow_result = small_queue.enqueue(999);
    std::cout << "Enqueue 999: " << (overflow_result ? "success" : "failed") << "\n";
    
    // Peek at front element
    int front_value;
    if (small_queue.front(front_value)) {
        std::cout << "Front element: " << front_value << "\n";
    }
    
    // Drain the queue
    std::cout << "\nDraining queue...\n";
    int value;
    while (small_queue.dequeue(value)) {
        std::cout << "Dequeued: " << value << " (remaining: " << small_queue.size() << ")\n";
    }
    
    std::cout << "\nQueue is now empty: " << (small_queue.empty() ? "true" : "false") << "\n";
    
    // Try to dequeue from empty queue
    std::cout << "\nTrying to dequeue from empty queue...\n";
    bool underflow_result = small_queue.dequeue(value);
    std::cout << "Dequeue: " << (underflow_result ? "success" : "failed") << "\n";
}

int main() {
    std::cout << "AtomicMPMCQueue Examples\n";
    std::cout << "========================\n\n";
    
    try {
        producer_consumer_example();
        high_frequency_example();
        capacity_and_bounds_example();
        
        std::cout << "\n🎉 All examples completed successfully!\n";
        std::cout << "\nKey takeaways:\n";
        std::cout << "• MPMC queue excels with multiple concurrent producers/consumers\n";
        std::cout << "• Fixed capacity provides predictable memory usage\n";
        std::cout << "• Lock-free design ensures high performance under contention\n";
        std::cout << "• Operations may fail when queue is full/empty - always check return values\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 