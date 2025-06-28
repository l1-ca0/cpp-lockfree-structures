#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <set>
#include <string>
#include <random>
#include "lockfree/atomic_queue.hpp"

using namespace lockfree;

void test_basic_queue_operations() {
    std::cout << "Testing basic queue operations...\n";
    
    AtomicQueue<int> queue;
    
    assert(queue.empty());
    assert(queue.size() == 0);
    
    // Test enqueue
    queue.enqueue(1);
    queue.enqueue(2);
    queue.enqueue(3);
    
    assert(!queue.empty());
    assert(queue.size() == 3);
    
    // Test front
    int front_val;
    assert(queue.front(front_val));
    assert(front_val == 1);
    assert(queue.size() == 3);  // front doesn't remove
    
    // Test dequeue
    int val;
    assert(queue.dequeue(val));
    assert(val == 1);
    assert(queue.size() == 2);
    
    assert(queue.dequeue(val));
    assert(val == 2);
    
    assert(queue.dequeue(val));
    assert(val == 3);
    
    assert(queue.empty());
    assert(queue.size() == 0);
    assert(!queue.dequeue(val));
    assert(!queue.front(front_val));
    
    std::cout << "Basic queue operations test passed!\n";
}

void test_string_queue() {
    std::cout << "Testing string queue operations...\n";
    
    AtomicQueue<std::string> queue;
    
    queue.enqueue("first");
    queue.enqueue("second");
    queue.enqueue(std::string("third"));
    
    std::string val;
    assert(queue.dequeue(val));
    assert(val == "first");
    
    assert(queue.dequeue(val));
    assert(val == "second");
    
    assert(queue.dequeue(val));
    assert(val == "third");
    
    std::cout << "String queue operations test passed!\n";
}

void test_concurrent_queue_operations() {
    std::cout << "Testing concurrent queue operations...\n";
    
    AtomicQueue<int> queue;
    constexpr int num_producers = 4;
    constexpr int num_consumers = 3;
    constexpr int items_per_producer = 500;
    
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::atomic<bool> production_done{false};
    
    std::vector<std::set<int>> consumed_by_thread(num_consumers);
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int value = p * items_per_producer + i;
                queue.enqueue(value);
                items_produced.fetch_add(1);
                
                // Small delay to increase contention
                if (i % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
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
            
            while (!production_done.load() || !queue.empty()) {
                if (queue.dequeue(value)) {
                    consumed_by_thread[c].insert(value);
                    my_consumed++;
                    items_consumed.fetch_add(1);
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
    std::cout << "Final queue size: " << queue.size() << "\n";
    
    // Verify all items were consumed exactly once
    std::set<int> all_consumed;
    for (const auto& thread_set : consumed_by_thread) {
        for (int value : thread_set) {
            assert(all_consumed.find(value) == all_consumed.end()); // No duplicates
            all_consumed.insert(value);
        }
    }
    
    assert(all_consumed.size() == items_consumed.load());
    assert(items_consumed.load() == items_produced.load() - queue.size());
    
    std::cout << "Concurrent queue operations test passed!\n";
}

void test_producer_consumer_pattern() {
    std::cout << "Testing producer-consumer pattern...\n";
    
    AtomicQueue<int> queue;
    constexpr int total_items = 1000;
    
    std::atomic<bool> producer_done{false};
    std::atomic<int> consumed_count{0};
    
    // Single producer
    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            queue.enqueue(i);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        producer_done.store(true);
    });
    
    // Single consumer
    std::thread consumer([&]() {
        int value;
        int last_value = -1;
        
        while (!producer_done.load() || !queue.empty()) {
            if (queue.dequeue(value)) {
                assert(value == last_value + 1); // FIFO order maintained
                last_value = value;
                consumed_count.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    assert(consumed_count.load() == total_items);
    assert(queue.empty());
    
    std::cout << "Producer-consumer pattern test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicQueue<std::pair<int, std::string>> queue;
    
    queue.emplace(1, "first");
    queue.emplace(2, "second");
    queue.emplace(3, "third");
    
    std::pair<int, std::string> val;
    assert(queue.dequeue(val));
    assert(val.first == 1 && val.second == "first");
    
    assert(queue.dequeue(val));
    assert(val.first == 2 && val.second == "second");
    
    assert(queue.dequeue(val));
    assert(val.first == 3 && val.second == "third");
    
    std::cout << "Emplace operations test passed!\n";
}

void test_queue_stress() {
    std::cout << "Testing queue stress test...\n";
    
    AtomicQueue<int> queue;
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 1000;
    
    std::atomic<int> enqueue_count{0};
    std::atomic<int> dequeue_count{0};
    
    std::vector<std::thread> threads;
    
    // Mixed enqueue/dequeue threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                if (op_dist(gen) < 60) {  // 60% enqueue operations
                    int value = t * operations_per_thread + i;
                    queue.enqueue(value);
                    enqueue_count.fetch_add(1);
                } else {  // 40% dequeue operations
                    int value;
                    if (queue.dequeue(value)) {
                        dequeue_count.fetch_add(1);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Enqueue operations: " << enqueue_count.load() << "\n";
    std::cout << "Dequeue operations: " << dequeue_count.load() << "\n";
    std::cout << "Final queue size: " << queue.size() << "\n";
    
    // Verify consistency
    assert(queue.size() == enqueue_count.load() - dequeue_count.load());
    
    std::cout << "Queue stress test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicQueue<std::unique_ptr<int>> queue;
    
    queue.enqueue(std::make_unique<int>(42));
    queue.enqueue(std::make_unique<int>(84));
    
    std::unique_ptr<int> ptr;
    assert(queue.dequeue(ptr));
    assert(*ptr == 42);
    
    assert(queue.dequeue(ptr));
    assert(*ptr == 84);
    
    assert(queue.empty());
    
    std::cout << "Move semantics test passed!\n";
}

int main() {
    std::cout << "AtomicQueue Tests\n";
    std::cout << "=================\n\n";
    
    test_basic_queue_operations();
    test_string_queue();
    test_concurrent_queue_operations();
    test_producer_consumer_pattern();
    test_emplace_operations();
    test_queue_stress();
    test_move_semantics();
    
    std::cout << "\nAll queue tests passed!\n";
    return 0;
}