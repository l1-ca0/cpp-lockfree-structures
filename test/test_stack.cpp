#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <set>
#include "lockfree/atomic_stack.hpp"

using namespace lockfree;

void test_basic_operations() {
    std::cout << "Testing basic stack operations...\n";
    
    AtomicStack<int> stack;
    
    assert(stack.empty());
    assert(stack.size() == 0);
    
    // Test push
    stack.push(1);
    stack.push(2);
    stack.push(3);
    
    assert(!stack.empty());
    assert(stack.size() == 3);
    
    // Test top
    int top_val;
    assert(stack.top(top_val));
    assert(top_val == 3);
    
    // Test pop
    int val;
    assert(stack.pop(val));
    assert(val == 3);
    assert(stack.size() == 2);
    
    assert(stack.pop(val));
    assert(val == 2);
    
    assert(stack.pop(val));
    assert(val == 1);
    
    assert(stack.empty());
    assert(!stack.pop(val));
    
    std::cout << "Basic operations test passed!\n";
}

void test_concurrent_operations() {
    std::cout << "Testing concurrent stack operations...\n";
    
    AtomicStack<int> stack;
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 1000;
    
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    std::vector<std::set<int>> popped_values(num_threads);
    
    std::vector<std::thread> threads;
    
    // Mixed push/pop threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // Push unique value
                int value = i * operations_per_thread + j;
                stack.push(value);
                push_count.fetch_add(1);
                
                // Try to pop
                int popped;
                if (stack.pop(popped)) {
                    popped_values[i].insert(popped);
                    pop_count.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Collect all popped values
    std::set<int> all_popped;
    for (const auto& thread_values : popped_values) {
        all_popped.insert(thread_values.begin(), thread_values.end());
    }
    
    std::cout << "Push count: " << push_count.load() << "\n";
    std::cout << "Pop count: " << pop_count.load() << "\n";
    std::cout << "Unique values popped: " << all_popped.size() << "\n";
    std::cout << "Stack size: " << stack.size() << "\n";
    
    // Verify no duplicates were popped
    size_t total_popped_individual = 0;
    for (const auto& thread_values : popped_values) {
        total_popped_individual += thread_values.size();
    }
    
    assert(total_popped_individual == all_popped.size()); // No duplicates
    assert(push_count.load() == pop_count.load() + stack.size());
    
    std::cout << "Concurrent operations test passed!\n";
}

void test_emplace() {
    std::cout << "Testing emplace functionality...\n";
    
    AtomicStack<std::pair<int, std::string>> stack;
    
    stack.emplace(1, "first");
    stack.emplace(2, "second");
    
    std::pair<int, std::string> val;
    assert(stack.pop(val));
    assert(val.first == 2 && val.second == "second");
    
    assert(stack.pop(val));
    assert(val.first == 1 && val.second == "first");
    
    std::cout << "Emplace test passed!\n";
}

int main() {
    std::cout << "AtomicStack Tests\n";
    std::cout << "=================\n\n";
    
    test_basic_operations();
    test_concurrent_operations();
    test_emplace();
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}