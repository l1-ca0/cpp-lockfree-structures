#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include "lockfree/atomic_priority_queue.hpp"

using namespace lockfree;

struct Task {
    int priority;
    std::string description;
    int task_id;
    
    Task(int p, const std::string& desc, int id) 
        : priority(p), description(desc), task_id(id) {}
    
    // Higher priority value = higher priority
    bool operator<(const Task& other) const {
        return priority < other.priority; // Min-heap behavior
    }
    
    bool operator>(const Task& other) const {
        return priority > other.priority; // For max-heap
    }
};

void test_basic_priority_queue() {
    std::cout << "=== Basic Priority Queue Operations ===\n";
    
    // Using std::greater for max-heap (highest priority first)
    AtomicPriorityQueue<Task, std::greater<Task>> pq;
    
    // Add tasks with different priorities
    pq.emplace(1, "Low priority task", 101);
    pq.emplace(5, "High priority task", 102);
    pq.emplace(3, "Medium priority task", 103);
    pq.emplace(10, "Critical task", 104);
    pq.emplace(2, "Low-medium priority task", 105);
    
    std::cout << "Added 5 tasks with various priorities\n";
    std::cout << "Queue size: " << pq.size() << "\n\n";
    
    // Process tasks in priority order
    std::cout << "Processing tasks in priority order:\n";
    Task task(0, "", 0);
    while (pq.pop(task)) {
        std::cout << "  Priority " << task.priority << ": " 
                  << task.description << " (ID: " << task.task_id << ")\n";
    }
    
    std::cout << "Final queue size: " << pq.size() << "\n\n";
}

void test_concurrent_task_processing() {
    std::cout << "=== Concurrent Task Processing ===\n";
    
    AtomicPriorityQueue<Task, std::greater<Task>> task_queue;
    constexpr int num_producers = 3;
    constexpr int num_consumers = 2;
    constexpr int tasks_per_producer = 100;
    
    std::atomic<int> tasks_produced{0};
    std::atomic<int> tasks_consumed{0};
    std::atomic<bool> production_done{false};
    
    // Producer threads - generate tasks with random priorities
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> priority_dist(1, 10);
            std::uniform_int_distribution<> delay_dist(1, 1000);
            
            for (int i = 0; i < tasks_per_producer; ++i) {
                int priority = priority_dist(gen);
                std::string desc = "Task from producer " + std::to_string(p) + 
                                  " item " + std::to_string(i);
                int task_id = p * tasks_per_producer + i;
                
                task_queue.emplace(priority, desc, task_id);
                tasks_produced.fetch_add(1);
                
                if (i % 25 == 0) {
                    std::cout << "Producer " << p << " created " << (i + 1) << " tasks\n";
                }
                
                // Variable delay to simulate realistic task generation
                std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(gen)));
            }
            
            std::cout << "Producer " << p << " finished\n";
        });
    }
    
    // Consumer threads - process highest priority tasks first
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&, c]() {
            Task task(0, "", 0);
            int my_processed = 0;
            int high_priority_count = 0;
            
            while (!production_done.load() || !task_queue.empty()) {
                if (task_queue.pop(task)) {
                    my_processed++;
                    tasks_consumed.fetch_add(1);
                    
                    if (task.priority >= 8) {
                        high_priority_count++;
                    }
                    
                    // Simulate task processing time (higher priority = faster processing)
                    int processing_time = std::max(1, 100 - (task.priority * 10));
                    std::this_thread::sleep_for(std::chrono::microseconds(processing_time));
                    
                    if (my_processed % 30 == 0) {
                        std::cout << "Consumer " << c << " processed " << my_processed 
                                  << " tasks (latest priority: " << task.priority << ")\n";
                    }
                } else {
                    // No tasks available, wait briefly
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            
            std::cout << "Consumer " << c << " finished. Processed " << my_processed 
                      << " tasks (" << high_priority_count << " high priority)\n";
        });
    }
    
    // Wait for producers to finish
    for (auto& producer : producers) {
        producer.join();
    }
    production_done.store(true);
    
    std::cout << "All producers finished. Tasks in queue: " << task_queue.size() << "\n";
    
    // Wait for consumers to finish
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    std::cout << "Total tasks produced: " << tasks_produced.load() << "\n";
    std::cout << "Total tasks consumed: " << tasks_consumed.load() << "\n";
    std::cout << "Final queue size: " << task_queue.size() << "\n\n";
}

void test_emergency_task_handling() {
    std::cout << "=== Emergency Task Handling ===\n";
    
    AtomicPriorityQueue<Task, std::greater<Task>> emergency_queue;
    
    // Simulate a system where emergency tasks can arrive at any time
    std::atomic<bool> simulation_running{true};
    std::atomic<int> emergency_tasks_handled{0};
    
    // Background task generator
    std::thread background_generator([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> priority_dist(1, 5); // Normal priorities
        std::uniform_int_distribution<> delay_dist(100, 500);
        
        int task_counter = 0;
        while (simulation_running.load()) {
            int priority = priority_dist(gen);
            emergency_queue.emplace(priority, "Background task", ++task_counter);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
        }
    });
    
    // Emergency task injector
    std::thread emergency_injector([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(1000, 3000);
        
        int emergency_id = 9000;
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            
            // Inject high-priority emergency task
            emergency_queue.emplace(15, "EMERGENCY TASK", ++emergency_id);
            std::cout << "🚨 EMERGENCY TASK " << emergency_id << " INJECTED!\n";
        }
    });
    
    // Task processor
    std::thread processor([&]() {
        Task task(0, "", 0);
        int processed_count = 0;
        
        while (simulation_running.load() || !emergency_queue.empty()) {
            if (emergency_queue.pop(task)) {
                processed_count++;
                
                if (task.priority >= 10) {
                    emergency_tasks_handled.fetch_add(1);
                    std::cout << "⚡ Processing EMERGENCY task " << task.task_id 
                              << " (priority: " << task.priority << ")\n";
                } else if (processed_count % 10 == 0) {
                    std::cout << "Processing regular task (priority: " << task.priority << ")\n";
                }
                
                // Emergency tasks get processed faster
                int processing_time = task.priority >= 10 ? 50 : 200;
                std::this_thread::sleep_for(std::chrono::milliseconds(processing_time));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        std::cout << "Processor finished. Total processed: " << processed_count << "\n";
    });
    
    // Run simulation for 10 seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));
    simulation_running.store(false);
    
    // Wait for all threads
    background_generator.join();
    emergency_injector.join();
    processor.join();
    
    std::cout << "Emergency tasks handled: " << emergency_tasks_handled.load() << "\n";
    std::cout << "Remaining tasks in queue: " << emergency_queue.size() << "\n\n";
}

int main() {
    std::cout << "Lock-free Priority Queue Example\n";
    std::cout << "=================================\n\n";
    
    test_basic_priority_queue();
    test_concurrent_task_processing();
    test_emergency_task_handling();
    
    std::cout << "All priority queue tests completed!\n";
    return 0;
}