#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <random>
#include <string>
#include <chrono>
#include "lockfree/atomic_ringbuffer.hpp"

using namespace lockfree;

void test_basic_ringbuffer_operations() {
    std::cout << "Testing basic ringbuffer operations...\n";
    
    AtomicRingBuffer<int, 8> buffer;
    
    assert(buffer.empty());
    assert(!buffer.full());
    assert(buffer.size() == 0);
    assert(buffer.capacity() == 8);
    
    // Test push operations
    for (int i = 1; i <= 5; ++i) {
        assert(buffer.push(i * 10));
    }
    
    assert(!buffer.empty());
    assert(!buffer.full());
    assert(buffer.size() == 5);
    
    // Test front and back
    int front_val, back_val;
    assert(buffer.front(front_val));
    assert(front_val == 10);  // First pushed
    
    assert(buffer.back(back_val));
    assert(back_val == 50);   // Last pushed
    
    // Test pop operations (FIFO order)
    int val;
    assert(buffer.pop(val));
    assert(val == 10);
    assert(buffer.size() == 4);
    
    assert(buffer.pop(val));
    assert(val == 20);
    assert(buffer.size() == 3);
    
    // Fill to capacity (we have 3 items, can add 5 more to reach capacity 8)
    for (int i = 6; i <= 10; ++i) {
        assert(buffer.push(i * 10));
    }
    
    assert(buffer.full());
    assert(buffer.size() == 8);
    
    // Should fail when full
    assert(!buffer.push(999));
    assert(buffer.size() == 8);
    
    // Empty the buffer
    while (buffer.pop(val)) {
        // Continue until empty
    }
    
    assert(buffer.empty());
    assert(buffer.size() == 0);
    assert(!buffer.pop(val));
    
    std::cout << "Basic ringbuffer operations test passed!\n";
}

void test_small_buffer_capacity() {
    std::cout << "Testing small buffer capacity handling...\n";
    
    AtomicRingBuffer<std::string, 4> buffer;
    
    // Fill to capacity
    assert(buffer.push("first"));
    assert(buffer.push("second"));
    assert(buffer.push("third"));
    assert(buffer.push("fourth"));
    
    assert(buffer.full());
    assert(buffer.size() == 4);
    
    // Should fail to push when full
    assert(!buffer.push("overflow"));
    
    // Pop one item
    std::string val;
    assert(buffer.pop(val));
    assert(val == "first");
    assert(!buffer.full());
    assert(buffer.size() == 3);
    
    // Should be able to push again
    assert(buffer.push("fifth"));
    assert(buffer.size() == 4);
    
    // Verify order maintained
    assert(buffer.pop(val));
    assert(val == "second");
    
    std::cout << "Small buffer capacity test passed!\n";
}

void test_concurrent_ringbuffer_operations() {
    std::cout << "Testing concurrent ringbuffer operations...\n";
    
    AtomicRingBuffer<int, 128> buffer;
    constexpr int num_producers = 3;
    constexpr int num_consumers = 2;
    constexpr int items_per_producer = 200;
    
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::atomic<bool> production_done{false};
    
    std::vector<std::thread> threads;
    
    // Producer threads
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int item = p * items_per_producer + i;
                
                while (!buffer.push(item)) {
                    std::this_thread::yield();
                }
                
                items_produced.fetch_add(1);
            }
        });
    }
    
    // Consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        threads.emplace_back([&, c]() {
            int item;
            int my_consumed = 0;
            
            while (!production_done.load() || !buffer.empty()) {
                if (buffer.pop(item)) {
                    my_consumed++;
                    items_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
            
            std::cout << "Consumer " << c << " consumed " << my_consumed << " items\n";
        });
    }
    
    // Wait for producers
    for (int i = 0; i < num_producers; ++i) {
        threads[i].join();
    }
    production_done.store(true);
    
    // Wait for consumers
    for (int i = num_producers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    std::cout << "Items produced: " << items_produced.load() << "\n";
    std::cout << "Items consumed: " << items_consumed.load() << "\n";
    std::cout << "Final buffer size: " << buffer.size() << "\n";
    
    // Verify consistency
    assert(items_consumed.load() <= items_produced.load());
    assert(buffer.size() == items_produced.load() - items_consumed.load());
    
    std::cout << "Concurrent ringbuffer operations test passed!\n";
}

void test_spsc_operations() {
    std::cout << "Testing SPSC (Single Producer Single Consumer) operations...\n";
    
    AtomicRingBuffer<int, 32> buffer;
    constexpr int num_items = 1000;
    
    std::atomic<bool> start_flag{false};
    std::atomic<int> items_sent{0};
    std::atomic<int> items_received{0};
    
    // Single producer
    std::thread producer([&]() {
        while (!start_flag.load()) {
            std::this_thread::yield();
        }
        
        for (int i = 0; i < num_items; ++i) {
            while (!buffer.spsc_push(i)) {
                std::this_thread::yield();
            }
            items_sent.fetch_add(1);
        }
    });
    
    // Single consumer
    std::thread consumer([&]() {
        while (!start_flag.load()) {
            std::this_thread::yield();
        }
        
        int value;
        int expected = 0;
        
        while (expected < num_items) {
            if (buffer.spsc_pop(value)) {
                assert(value == expected); // Verify FIFO order
                expected++;
                items_received.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "SPSC test completed in " << duration.count() << " μs\n";
    std::cout << "Items sent: " << items_sent.load() << "\n";
    std::cout << "Items received: " << items_received.load() << "\n";
    
    assert(items_sent.load() == num_items);
    assert(items_received.load() == num_items);
    assert(buffer.empty());
    
    std::cout << "SPSC operations test passed!\n";
}

void test_emplace_operations() {
    std::cout << "Testing emplace operations...\n";
    
    AtomicRingBuffer<std::pair<int, std::string>, 8> buffer;
    
    assert(buffer.emplace(1, "first"));
    assert(buffer.emplace(2, "second"));
    assert(buffer.emplace(3, "third"));
    
    assert(buffer.size() == 3);
    
    std::pair<int, std::string> item;
    assert(buffer.pop(item));
    assert(item.first == 1 && item.second == "first");
    
    assert(buffer.pop(item));
    assert(item.first == 2 && item.second == "second");
    
    assert(buffer.pop(item));
    assert(item.first == 3 && item.second == "third");
    
    assert(buffer.empty());
    
    std::cout << "Emplace operations test passed!\n";
}

void test_wraparound_behavior() {
    std::cout << "Testing wraparound behavior...\n";
    
    AtomicRingBuffer<int, 4> buffer;
    
    // Fill buffer
    for (int i = 1; i <= 4; ++i) {
        assert(buffer.push(i));
    }
    assert(buffer.full());
    
    // Pop 2 items
    int val;
    assert(buffer.pop(val));
    assert(val == 1);
    assert(buffer.pop(val));
    assert(val == 2);
    
    assert(buffer.size() == 2);
    
    // Push 2 more items (should wrap around)
    assert(buffer.push(5));
    assert(buffer.push(6));
    assert(buffer.full());
    
    // Verify order is maintained
    assert(buffer.pop(val));
    assert(val == 3);
    assert(buffer.pop(val));
    assert(val == 4);
    assert(buffer.pop(val));
    assert(val == 5);
    assert(buffer.pop(val));
    assert(val == 6);
    
    assert(buffer.empty());
    
    std::cout << "Wraparound behavior test passed!\n";
}

void test_front_back_operations() {
    std::cout << "Testing front and back operations...\n";
    
    AtomicRingBuffer<std::string, 8> buffer;
    
    // Empty buffer
    std::string val;
    assert(!buffer.front(val));
    assert(!buffer.back(val));
    
    // Single item
    buffer.push("only");
    assert(buffer.front(val));
    assert(val == "only");
    assert(buffer.back(val));
    assert(val == "only");
    
    // Multiple items
    buffer.push("second");
    buffer.push("third");
    
    assert(buffer.front(val));
    assert(val == "only");    // First in
    assert(buffer.back(val));
    assert(val == "third");   // Last in
    
    // Pop front item
    buffer.pop(val);
    assert(val == "only");
    
    assert(buffer.front(val));
    assert(val == "second");  // New front
    assert(buffer.back(val));
    assert(val == "third");   // Same back
    
    std::cout << "Front and back operations test passed!\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics...\n";
    
    AtomicRingBuffer<std::unique_ptr<int>, 8> buffer;
    
    // Test push with move
    assert(buffer.push(std::make_unique<int>(100)));
    assert(buffer.push(std::make_unique<int>(200)));
    
    std::unique_ptr<int> ptr;
    assert(buffer.pop(ptr));
    assert(*ptr == 100);
    
    assert(buffer.pop(ptr));
    assert(*ptr == 200);
    
    assert(buffer.empty());
    
    std::cout << "Move semantics test passed!\n";
}

void test_stress_operations() {
    std::cout << "Testing stress operations...\n";
    
    AtomicRingBuffer<int, 64> buffer;
    constexpr int num_threads = 6;
    constexpr int operations_per_thread = 1000;
    
    std::atomic<int> push_attempts{0};
    std::atomic<int> successful_pushes{0};
    std::atomic<int> pop_attempts{0};
    std::atomic<int> successful_pops{0};
    
    std::vector<std::thread> threads;
    
    // Stress test with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                if (op_dist(gen) < 60) {  // 60% push operations
                    push_attempts.fetch_add(1);
                    int value = t * operations_per_thread + i;
                    if (buffer.push(value)) {
                        successful_pushes.fetch_add(1);
                    }
                } else {  // 40% pop operations
                    pop_attempts.fetch_add(1);
                    int value;
                    if (buffer.pop(value)) {
                        successful_pops.fetch_add(1);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Push attempts: " << push_attempts.load() 
              << ", successful: " << successful_pushes.load() << "\n";
    std::cout << "Pop attempts: " << pop_attempts.load() 
              << ", successful: " << successful_pops.load() << "\n";
    std::cout << "Final buffer size: " << buffer.size() << "\n";
    
    // Verify consistency
    assert(buffer.size() == successful_pushes.load() - successful_pops.load());
    assert(successful_pops.load() <= successful_pushes.load());
    
    std::cout << "Stress operations test passed!\n";
}

int main() {
    std::cout << "AtomicRingBuffer Tests\n";
    std::cout << "======================\n\n";
    
    test_basic_ringbuffer_operations();
    test_small_buffer_capacity();
    test_concurrent_ringbuffer_operations();
    test_spsc_operations();
    test_emplace_operations();
    test_wraparound_behavior();
    test_front_back_operations();
    test_move_semantics();
    test_stress_operations();
    
    std::cout << "\nAll ringbuffer tests passed!\n";
    std::cout << "\nNote: This RingBuffer implementation provides both general\n";
    std::cout << "concurrent operations and optimized SPSC variants. The buffer\n";
    std::cout << "size must be a power of 2 for optimal performance.\n";
    
    return 0;
}