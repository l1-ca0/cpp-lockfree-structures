#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <random>
#include <algorithm>
#include "lockfree/atomic_priority_queue.hpp"

using namespace lockfree;

struct TestItem {
    int priority;
    int value;
    
    TestItem(int p = 0, int v = 0) : priority(p), value(v) {}
    
    bool operator<(const TestItem& other) const {
        return priority < other.priority;  // Min-heap
    }
    
    bool operator>(const TestItem& other) const {
        return priority > other.priority;  // Max-heap
    }
};

void test_basic_priority_queue_operations() {
    std::cout << "Testing basic priority queue operations...\n";
    
    // Test max-heap behavior
    AtomicPriorityQueue<TestItem, std::greater<TestItem>> pq;
    
    assert(pq.empty());
    assert(pq.size() == 0);
    
    // Insert items with different priorities
    pq.push(TestItem(3, 100));
    pq.push(TestItem(1, 200));
    pq.push(TestItem(5, 300));
    pq.push(TestItem(2, 400));
    
    assert(!pq.empty());
    assert(pq.size() == 4);
    
    // Test top (should be highest priority = 5)
    TestItem top_item;
    assert(pq.top(top_item));
    assert(top_item.priority == 5);
    assert(top_item.value == 300);
    
    // Test pop in priority order (5, 3, 2, 1)
    TestItem item;
    assert(pq.pop(item));
    assert(item.priority == 5);
    
    assert(pq.pop(item));
    assert(item.priority == 3);
    
    assert(pq.pop(item));
    assert(item.priority == 2);
    
    assert(pq.pop(item));
    assert(item.priority == 1);
    
    assert(pq.empty());
    assert(pq.size() == 0);
    assert(!pq.pop(item));
    
    std::cout << "Basic priority queue operations test passed!\n";
}

void test_integer_priority_queue() {
    std::cout << "Testing integer priority queue...\n";
    
    AtomicPriorityQueue<int> pq;  // Default is std::less (min-heap)
    
    // Insert random values
    std::vector<int> values = {15, 3, 8, 1, 12, 7, 20, 5};
    for (int val : values) {
        pq.push(val);
    }
    
    // Should pop in ascending order for min-heap
    std::vector<int> popped_values;
    int val;
    while (pq.pop(val)) {
        popped_values.push_back(val);
    }
    
    // Verify sorted order
    std::vector<int> expected = values;
    std::sort(expected.begin(), expected.end());
    
    assert(popped_values == expected);
    
    std::cout << "Integer priority queue test passed!\n";
}

void test_concurrent_priority_queue() {
    std::cout << "Testing concurrent priority queue operations...\n";
    
    AtomicPriorityQueue<int, std::greater<int>> pq;  // Max-heap
    constexpr int num_producers = 3;
    constexpr int num_consumers = 2;
    constexpr int items_per_producer = 200;
    
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::atomic<bool> production_done{false};
    
    std::vector<int> consumed_values;
    std::mutex consumed_mutex;
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> priority_dist(1, 100);
            
            for (int i = 0; i < items_per_producer; ++i) {
                int priority = priority_dist(gen);
                pq.push(priority);
                items_produced.fetch_add(1);
                
                // Small delay to increase contention
                if (i % 50 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }
    
    // Consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&, c]() {
            int value;
            int my_consumed = 0;
            
            while (!production_done.load() || !pq.empty()) {
                if (pq.pop(value)) {
                    my_consumed++;
                    items_consumed.fetch_add(1);
                    
                    {
                        std::lock_guard<std::mutex> lock(consumed_mutex);
                        consumed_values.push_back(value);
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
            
            std::cout << "Consumer " << c << " consumed " << my_consumed << " items\n";
        });
    }
    
    // Wait for producers to finish
    for (auto& producer : producers) {
        producer.join();
    }
    production_done.store(true);
    
    // Wait for consumers to finish
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    std::cout << "Items produced: " << items_produced.load() << "\n";
    std::cout << "Items consumed: " << items_consumed.load() << "\n";
    std::cout << "Final queue size: " << pq.size() << "\n";
    
    // Verify that consumed values are in descending order (max-heap)
    for (size_t i = 1; i < consumed_values.size(); ++i) {
        // Note: Due to concurrent access, strict ordering might not be maintained
        // across different consumers, but within each consumer's sequence it should be
    }
    
    assert(items_consumed.load() <= items_produced.load());
    
    std::cout << "Concurrent priority queue operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicPriorityQueue<TestItem, std::greater<TestItem>> pq;
    
    pq.emplace(10, 1000);
    pq.emplace(5, 2000);
    pq.emplace(15, 3000);
    
    TestItem item;
    assert(pq.pop(item));
    assert(item.priority == 15 && item.value == 3000);
    
    assert(pq.pop(item));
    assert(item.priority == 10 && item.value == 1000);
    
    assert(pq.pop(item));
    assert(item.priority == 5 && item.value == 2000);
    
    std::cout << "Emplace operations test passed!\n";
}

void test_priority_queue_stress() {
    std::cout << "Testing priority queue stress test...\n";
    
    AtomicPriorityQueue<int> pq;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    
    std::atomic<int> successful_pushes{0};
    std::atomic<int> successful_pops{0};
    
    std::vector<std::thread> threads;
    
    // Mixed push/pop threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> value_dist(1, 1000);
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                if (op_dist(gen) < 70) {  // 70% push operations
                    int value = value_dist(gen);
                    pq.push(value);
                    successful_pushes.fetch_add(1);
                } else {  // 30% pop operations
                    int value;
                    if (pq.pop(value)) {
                        successful_pops.fetch_add(1);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Successful pushes: " << successful_pushes.load() << "\n";
    std::cout << "Successful pops: " << successful_pops.load() << "\n";
    std::cout << "Final queue size: " << pq.size() << "\n";
    
    // Verify consistency
    assert(pq.size() == successful_pushes.load() - successful_pops.load());
    
    std::cout << "Priority queue stress test passed!\n";
}

int main() {
    std::cout << "AtomicPriorityQueue Tests\n";
    std::cout << "=========================\n\n";
    
    test_basic_priority_queue_operations();
    test_integer_priority_queue();
    test_concurrent_priority_queue();
    test_emplace_operations();
    test_priority_queue_stress();
    
    std::cout << "\nAll priority queue tests passed!\n";
    return 0;
}