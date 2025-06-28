#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <set>
#include <algorithm>
#include <chrono>
#include "lockfree/atomic_mpmc_queue.hpp"

using namespace lockfree;

void test_basic_operations() {
    std::cout << "Testing basic operations... ";
    
    AtomicMPMCQueue<int, 8> queue;
    
    // Test initial state
    assert(queue.empty());
    assert(!queue.full());
    assert(queue.size() == 0);
    assert(queue.capacity() == 8);
    
    // Test enqueue
    assert(queue.enqueue(42));
    assert(!queue.empty());
    assert(queue.size() == 1);
    
    // Test front
    int front_value;
    assert(queue.front(front_value));
    assert(front_value == 42);
    
    // Test dequeue
    int value;
    assert(queue.dequeue(value));
    assert(value == 42);
    assert(queue.empty());
    assert(queue.size() == 0);
    
    // Test dequeue from empty queue
    assert(!queue.dequeue(value));
    
    std::cout << "✓ Passed\n";
}

void test_move_semantics() {
    std::cout << "Testing move semantics... ";
    
    AtomicMPMCQueue<std::string, 16> queue;
    
    // Test move enqueue
    std::string str = "hello world";
    assert(queue.enqueue(std::move(str)));
    // str should be moved from (implementation dependent)
    
    // Test dequeue
    std::string result;
    assert(queue.dequeue(result));
    assert(result == "hello world");
    
    std::cout << "✓ Passed\n";
}

void test_emplace() {
    std::cout << "Testing emplace functionality... ";
    
    AtomicMPMCQueue<std::pair<int, std::string>, 16> queue;
    
    // Test emplace
    assert(queue.emplace(42, "test"));
    assert(queue.size() == 1);
    
    std::pair<int, std::string> result;
    assert(queue.dequeue(result));
    assert(result.first == 42);
    assert(result.second == "test");
    
    std::cout << "✓ Passed\n";
}

void test_capacity_limits() {
    std::cout << "Testing capacity limits... ";
    
    AtomicMPMCQueue<int, 4> queue;
    
    // Fill to capacity
    for (int i = 0; i < 4; ++i) {
        assert(queue.enqueue(i));
    }
    
    assert(queue.full());
    assert(queue.size() == 4);
    
    // Try to exceed capacity
    assert(!queue.enqueue(999));
    assert(queue.size() == 4);
    
    // Drain queue
    int value;
    for (int i = 0; i < 4; ++i) {
        assert(queue.dequeue(value));
        assert(value == i);
    }
    
    assert(queue.empty());
    assert(queue.size() == 0);
    
    std::cout << "✓ Passed\n";
}

void test_fifo_ordering() {
    std::cout << "Testing FIFO ordering... ";
    
    AtomicMPMCQueue<int, 32> queue;
    
    // Enqueue sequence
    const int count = 20;
    for (int i = 0; i < count; ++i) {
        assert(queue.enqueue(i));
    }
    
    // Verify FIFO order
    for (int i = 0; i < count; ++i) {
        int value;
        assert(queue.dequeue(value));
        assert(value == i);
    }
    
    assert(queue.empty());
    
    std::cout << "✓ Passed\n";
}

void test_single_producer_single_consumer() {
    std::cout << "Testing single producer single consumer... ";
    
    AtomicMPMCQueue<int, 1024> queue;
    const int count = 10000;
    std::atomic<bool> producer_done{false};
    std::vector<int> consumed;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < count; ++i) {
            while (!queue.enqueue(i)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true);
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (!producer_done.load() || !queue.empty()) {
            if (queue.dequeue(value)) {
                consumed.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all items were consumed in order
    assert(consumed.size() == count);
    for (int i = 0; i < count; ++i) {
        assert(consumed[i] == i);
    }
    
    std::cout << "✓ Passed\n";
}

void test_multiple_producers_single_consumer() {
    std::cout << "Testing multiple producers single consumer... ";
    
    AtomicMPMCQueue<int, 2048> queue;
    const int num_producers = 4;
    const int items_per_producer = 1000;
    std::atomic<int> producers_done{0};
    std::vector<int> consumed;
    std::vector<std::thread> producers;
    
    // Producer threads
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int value = p * items_per_producer + i;
                while (!queue.enqueue(value)) {
                    std::this_thread::yield();
                }
            }
            producers_done.fetch_add(1);
        });
    }
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (producers_done.load() < num_producers || !queue.empty()) {
            if (queue.dequeue(value)) {
                consumed.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    // Verify all items were consumed
    assert(consumed.size() == num_producers * items_per_producer);
    
    // Verify all expected values are present
    std::sort(consumed.begin(), consumed.end());
    for (int i = 0; i < num_producers * items_per_producer; ++i) {
        assert(consumed[i] == i);
    }
    
    std::cout << "✓ Passed\n";
}

void test_single_producer_multiple_consumers() {
    std::cout << "Testing single producer multiple consumers... ";
    
    AtomicMPMCQueue<int, 2048> queue;
    const int count = 4000;
    const int num_consumers = 4;
    std::atomic<bool> producer_done{false};
    std::atomic<int> total_consumed{0};
    std::vector<std::thread> consumers;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < count; ++i) {
            while (!queue.enqueue(i)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true);
    });
    
    // Consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            int value;
            while (!producer_done.load() || !queue.empty()) {
                if (queue.dequeue(value)) {
                    total_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    producer.join();
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    // Verify all items were consumed exactly once
    assert(total_consumed.load() == count);
    
    std::cout << "✓ Passed\n";
}

void test_multiple_producers_multiple_consumers() {
    std::cout << "Testing multiple producers multiple consumers... ";
    
    AtomicMPMCQueue<int, 2048> queue;
    const int num_producers = 3;
    const int num_consumers = 3;
    const int items_per_producer = 1000;
    std::atomic<int> producers_done{0};
    std::atomic<int> total_consumed{0};
    std::vector<std::thread> threads;
    
    // Producer threads
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int value = p * items_per_producer + i;
                while (!queue.enqueue(value)) {
                    std::this_thread::yield();
                }
            }
            producers_done.fetch_add(1);
        });
    }
    
    // Consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        threads.emplace_back([&]() {
            int value;
            while (producers_done.load() < num_producers || !queue.empty()) {
                if (queue.dequeue(value)) {
                    total_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all items were consumed exactly once
    assert(total_consumed.load() == num_producers * items_per_producer);
    
    std::cout << "✓ Passed\n";
}

void test_stress_with_random_operations() {
    std::cout << "Testing stress with random operations... ";
    
    AtomicMPMCQueue<int, 512> queue;
    const int num_threads = 8;
    const int operations_per_thread = 1000;
    std::atomic<int> total_enqueued{0};
    std::atomic<int> total_dequeued{0};
    std::vector<std::thread> threads;
    
    // Mixed producer/consumer threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 1);  // 0 = enqueue, 1 = dequeue
            
            for (int i = 0; i < operations_per_thread; ++i) {
                if (op_dist(gen) == 0) {
                    // Try to enqueue
                    int value = t * operations_per_thread + i;
                    if (queue.enqueue(value)) {
                        total_enqueued.fetch_add(1);
                    }
                } else {
                    // Try to dequeue
                    int value;
                    if (queue.dequeue(value)) {
                        total_dequeued.fetch_add(1);
                    }
                }
                
                // Small random delay
                if (i % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Drain remaining items
    int value;
    while (queue.dequeue(value)) {
        total_dequeued.fetch_add(1);
    }
    
    // Verify consistency
    assert(total_enqueued.load() == total_dequeued.load());
    assert(queue.empty());
    
    std::cout << "✓ Passed (enqueued: " << total_enqueued.load() 
              << ", dequeued: " << total_dequeued.load() << ")\n";
}

void test_front_during_concurrent_access() {
    std::cout << "Testing front() during concurrent access... ";
    
    AtomicMPMCQueue<int, 256> queue;
    std::atomic<bool> keep_running{true};
    std::atomic<int> front_reads{0};
    std::atomic<int> successful_front_reads{0};
    
    // Producer thread
    std::thread producer([&]() {
        int value = 0;
        while (keep_running.load()) {
            if (queue.enqueue(value++)) {
                // Successfully enqueued
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (keep_running.load()) {
            if (queue.dequeue(value)) {
                // Successfully dequeued
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Front reader thread
    std::thread front_reader([&]() {
        int value;
        while (keep_running.load()) {
            front_reads.fetch_add(1);
            if (queue.front(value)) {
                successful_front_reads.fetch_add(1);
            }
        }
    });
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    keep_running.store(false);
    
    producer.join();
    consumer.join();
    front_reader.join();
    
    std::cout << "✓ Passed (front reads: " << front_reads.load() 
              << ", successful: " << successful_front_reads.load() << ")\n";
}

void test_power_of_two_requirement() {
    std::cout << "Testing power of two requirement... ";
    
    // These should compile fine (powers of 2)
    AtomicMPMCQueue<int, 2> queue2;
    AtomicMPMCQueue<int, 4> queue4;
    AtomicMPMCQueue<int, 8> queue8;
    AtomicMPMCQueue<int, 16> queue16;
    AtomicMPMCQueue<int, 1024> queue1024;
    
    // Test that they work
    assert(queue2.capacity() == 2);
    assert(queue4.capacity() == 4);
    assert(queue8.capacity() == 8);
    assert(queue16.capacity() == 16);
    assert(queue1024.capacity() == 1024);
    
    std::cout << "✓ Passed\n";
}

int main() {
    std::cout << "AtomicMPMCQueue Tests\n";
    std::cout << "=====================\n\n";
    
    try {
        test_basic_operations();
        test_move_semantics();
        test_emplace();
        test_capacity_limits();
        test_fifo_ordering();
        test_power_of_two_requirement();
        test_single_producer_single_consumer();
        test_multiple_producers_single_consumer();
        test_single_producer_multiple_consumers();
        test_multiple_producers_multiple_consumers();
        test_stress_with_random_operations();
        test_front_during_concurrent_access();
        
        std::cout << "\n🎉 All tests passed!\n";
        std::cout << "\nAtomicMPMCQueue is working correctly with:\n";
        std::cout << "• Basic operations (enqueue, dequeue, front)\n";
        std::cout << "• Move semantics and emplace functionality\n";
        std::cout << "• Capacity limits and bounds checking\n";
        std::cout << "• FIFO ordering guarantees\n";
        std::cout << "• Single producer/consumer scenarios\n";
        std::cout << "• Multiple producer/consumer scenarios\n";
        std::cout << "• High-contention concurrent access\n";
        std::cout << "• Thread safety under stress conditions\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
} 