#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include "lockfree/atomic_queue.hpp"

using namespace lockfree;

int main() {
    AtomicQueue<std::string> queue;
    constexpr int num_producers = 3;
    constexpr int num_consumers = 2;
    constexpr int items_per_producer = 500;
    
    std::cout << "Lock-free Queue Example\n";
    std::cout << "=======================\n\n";
    
    std::atomic<bool> start_flag{false};
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            for (int j = 0; j < items_per_producer; ++j) {
                std::string item = "Producer" + std::to_string(i) + "_Item" + std::to_string(j);
                queue.enqueue(std::move(item));
                items_produced.fetch_add(1);
                
                // Small delay to make it more realistic
                if (j % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }
    
    // Consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&, i]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            
            std::string item;
            int my_consumed = 0;
            
            while (true) {
                if (queue.dequeue(item)) {
                    my_consumed++;
                    items_consumed.fetch_add(1);
                    
                    // Process item (just print occasionally)
                    if (my_consumed % 200 == 0) {
                        std::cout << "Consumer " << i << " processed: " << item << "\n";
                    }
                } else {
                    // Check if all producers are done
                    if (items_produced.load() >= num_producers * items_per_producer &&
                        queue.empty()) {
                        break;
                    }
                    
                    // Queue is empty but producers might still be working
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
            
            std::cout << "Consumer " << i << " finished. Processed " << my_consumed << " items.\n";
        });
    }
    
    std::cout << "Starting concurrent queue operations...\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    // Start all threads
    start_flag.store(true);
    
    // Wait for all producers to finish
    for (auto& t : producers) {
        t.join();
    }
    
    std::cout << "All producers finished. Items produced: " << items_produced.load() << "\n";
    std::cout << "Queue size: " << queue.size() << "\n";
    
    // Wait for all consumers to finish
    for (auto& t : consumers) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "All consumers finished. Items consumed: " << items_consumed.load() << "\n";
    std::cout << "Final queue size: " << queue.size() << "\n";
    std::cout << "Time taken: " << duration.count() << " ms\n";
    
    // Test front() functionality
    std::string front_item;
    if (queue.front(front_item)) {
        std::cout << "Front element: " << front_item << "\n";
    } else {
        std::cout << "Queue is empty\n";
    }
    
    return 0;
}