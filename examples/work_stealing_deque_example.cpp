#include <lockfree/atomic_work_stealing_deque.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

/**
 * Example: Work-Stealing Task Scheduler
 * 
 * This demonstrates a typical use case for work-stealing deques:
 * - A main thread generates tasks and puts them in its own deque
 * - Worker threads steal tasks from the main thread's deque when idle
 * - The main thread works on its own tasks (LIFO) while workers steal old tasks (FIFO)
 */

struct Task {
    int id;
    int work_amount;
    
    Task(int id, int work) : id(id), work_amount(work) {}
};

// Simulate some work
void process_task(const Task& task) {
    // Simulate CPU work
    volatile int sum = 0;
    for (int i = 0; i < task.work_amount * 1000; ++i) {
        sum += i;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(task.work_amount));
}

int main() {
    std::cout << "Work-Stealing Deque Example\n";
    std::cout << "============================\n\n";
    
    // Create the work-stealing deque
    lockfree::AtomicWorkStealingDeque<Task> work_deque;
    
    // Statistics
    std::atomic<int> tasks_completed{0};
    std::atomic<int> tasks_stolen{0};
    std::atomic<int> tasks_processed_by_owner{0};
    
    const int num_workers = 3;
    const int total_tasks = 100;
    
    std::cout << "Starting work-stealing scheduler with " << num_workers << " worker threads\n";
    std::cout << "Total tasks to process: " << total_tasks << "\n\n";
    
    // Start worker threads (thieves)
    std::vector<std::thread> workers;
    std::atomic<bool> stop_workers{false};
    
    for (int i = 0; i < num_workers; ++i) {
        workers.emplace_back([&, worker_id = i]() {
            std::cout << "Worker " << worker_id << " started\n";
            
            while (!stop_workers.load()) {
                // Try to steal a task
                Task* task = work_deque.steal();
                if (task) {
                    std::cout << "Worker " << worker_id << " stole task " << task->id << "\n";
                    process_task(*task);
                    delete task;
                    tasks_stolen.fetch_add(1);
                    tasks_completed.fetch_add(1);
                } else {
                    // No work available, sleep briefly
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            
            std::cout << "Worker " << worker_id << " finished\n";
        });
    }
    
    // Main thread (owner) generates and processes tasks
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> work_dist(1, 10);
    
    std::cout << "\nMain thread starting task generation and processing...\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Generate tasks
    for (int i = 0; i < total_tasks; ++i) {
        int work_amount = work_dist(gen);
        work_deque.push_bottom(Task(i, work_amount));
        std::cout << "Generated task " << i << " (work: " << work_amount << ")\n";
        
        // Occasionally process our own tasks
        if (i % 3 == 0) {
            Task* task = work_deque.pop_bottom();
            if (task) {
                std::cout << "Owner processed task " << task->id << " locally\n";
                process_task(*task);
                delete task;
                tasks_processed_by_owner.fetch_add(1);
                tasks_completed.fetch_add(1);
            }
        }
        
        // Brief pause to allow stealing
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::cout << "\nTask generation complete. Processing remaining tasks...\n\n";
    
    // Process remaining tasks in our deque
    Task* task;
    while ((task = work_deque.pop_bottom()) != nullptr) {
        std::cout << "Owner processed remaining task " << task->id << "\n";
        process_task(*task);
        delete task;
        tasks_processed_by_owner.fetch_add(1);
        tasks_completed.fetch_add(1);
    }
    
    // Wait for all tasks to complete
    while (tasks_completed.load() < total_tasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Stop workers
    stop_workers.store(true);
    for (auto& worker : workers) {
        worker.join();
    }
    
    std::cout << "\n=== Work-Stealing Results ===\n";
    std::cout << "Total tasks completed: " << tasks_completed.load() << "\n";
    std::cout << "Tasks processed by owner: " << tasks_processed_by_owner.load() << "\n";
    std::cout << "Tasks stolen by workers: " << tasks_stolen.load() << "\n";
    std::cout << "Total execution time: " << duration.count() << " ms\n";
    std::cout << "Work distribution: " 
              << (100.0 * tasks_stolen.load() / total_tasks) << "% stolen, "
              << (100.0 * tasks_processed_by_owner.load() / total_tasks) << "% owner\n";
    
    std::cout << "\nWork-stealing deque example completed successfully!\n";
    
    return 0;
} 