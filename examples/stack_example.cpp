#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include "lockfree/atomic_stack.hpp"

using namespace lockfree;

int main() {
    AtomicStack<int> stack;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 1000;
    
    std::cout << "Lock-free Stack Example\n";
    std::cout << "=======================\n\n";
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < num_threads; ++i) {
        producers.emplace_back([&stack, i, operations_per_thread]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 1000);
            
            for (int j = 0; j < operations_per_thread; ++j) {
                int value = i * operations_per_thread + j;
                stack.push(value);
                
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::microseconds(dis(gen) % 10));
            }
        });
    }
    
    // Consumer threads
    std::vector<std::thread> consumers;
    std::atomic<int> total_consumed{0};
    
    for (int i = 0; i < num_threads; ++i) {
        consumers.emplace_back([&stack, &total_consumed]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 1000);
            
            int value;
            int consumed = 0;
            
            while (consumed < 250) { // Each consumer tries to get 250 items
                if (stack.pop(value)) {
                    consumed++;
                    total_consumed.fetch_add(1);
                } else {
                    // Stack is empty, wait a bit
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        });
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Wait for all producers to finish
    for (auto& t : producers) {
        t.join();
    }
    
    std::cout << "All producers finished. Stack size: " << stack.size() << "\n";
    
    // Wait for all consumers to finish
    for (auto& t : consumers) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "All consumers finished. Total consumed: " << total_consumed.load() << "\n";
    std::cout << "Remaining in stack: " << stack.size() << "\n";
    std::cout << "Time taken: " << duration.count() << " ms\n";
    
    // Test top() functionality
    int top_value;
    if (stack.top(top_value)) {
        std::cout << "Top element: " << top_value << "\n";
    } else {
        std::cout << "Stack is empty\n";
    }
    
    return 0;
}