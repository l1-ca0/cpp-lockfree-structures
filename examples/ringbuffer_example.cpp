#include <iostream>
#include <cassert>
#include <iomanip>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include "lockfree/atomic_ringbuffer.hpp"

using namespace lockfree;

void test_basic_ringbuffer_operations() {
    std::cout << "=== Basic RingBuffer Operations ===\n";
    
    AtomicRingBuffer<int, 8> buffer;
    
    std::cout << "Initial state:\n";
    std::cout << "  Empty: " << buffer.empty() << "\n";
    std::cout << "  Full: " << buffer.full() << "\n";
    std::cout << "  Size: " << buffer.size() << "\n";
    std::cout << "  Capacity: " << buffer.capacity() << "\n\n";
    
    // Test push operations
    std::cout << "Pushing elements...\n";
    for (int i = 1; i <= 5; ++i) {
        bool success = buffer.push(i * 10);
        std::cout << "  Push " << (i * 10) << ": " 
                  << (success ? "Success" : "Failed") << "\n";
    }
    
    std::cout << "After pushes - Size: " << buffer.size() << "\n";
    
    // Test front and back
    int front_val, back_val;
    if (buffer.front(front_val)) {
        std::cout << "Front element: " << front_val << "\n";
    }
    if (buffer.back(back_val)) {
        std::cout << "Back element: " << back_val << "\n";
    }
    
    // Test pop operations
    std::cout << "\nPopping elements...\n";
    int value;
    while (buffer.pop(value)) {
        std::cout << "  Popped: " << value << "\n";
    }
    
    std::cout << "After pops - Size: " << buffer.size() << "\n";
    std::cout << "Empty: " << buffer.empty() << "\n\n";
}

void test_ringbuffer_capacity_limits() {
    std::cout << "=== RingBuffer Capacity Limits ===\n";
    
    AtomicRingBuffer<int, 4> small_buffer;
    
    std::cout << "Filling buffer to capacity...\n";
    for (int i = 0; i < 6; ++i) {
        bool success = small_buffer.push(i);
        std::cout << "  Push " << i << ": " 
                  << (success ? "Success" : "Failed (buffer full)") 
                  << " - Size: " << small_buffer.size() << "\n";
    }
    
    std::cout << "Buffer full: " << small_buffer.full() << "\n";
    
    // Empty the buffer
    std::cout << "\nEmptying buffer...\n";
    int value;
    while (small_buffer.pop(value)) {
        std::cout << "  Popped: " << value << "\n";
    }
    
    std::cout << "Buffer empty: " << small_buffer.empty() << "\n\n";
}

void test_producer_consumer_pattern() {
    std::cout << "=== Producer-Consumer Pattern ===\n";
    
    AtomicRingBuffer<std::string, 32> buffer;
    constexpr int total_items = 100;
    
    std::atomic<bool> producer_done{false};
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            std::string item = "Item_" + std::to_string(i);
            
            while (!buffer.push(std::move(item))) {
                // Buffer full, wait a bit
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            
            items_produced.fetch_add(1);
            
            if (i % 20 == 0) {
                std::cout << "Produced " << (i + 1) << " items\n";
            }
            
            // Simulate variable production rate
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        producer_done.store(true);
        std::cout << "Producer finished\n";
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        std::string item;
        int consumed = 0;
        
        while (!producer_done.load() || !buffer.empty()) {
            if (buffer.pop(item)) {
                consumed++;
                items_consumed.fetch_add(1);
                
                if (consumed % 25 == 0) {
                    std::cout << "Consumed " << consumed << " items (latest: " << item << ")\n";
                }
            } else {
                // Buffer empty, wait a bit
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        
        std::cout << "Consumer finished. Total consumed: " << consumed << "\n";
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "Final state:\n";
    std::cout << "  Items produced: " << items_produced.load() << "\n";
    std::cout << "  Items consumed: " << items_consumed.load() << "\n";
    std::cout << "  Remaining in buffer: " << buffer.size() << "\n\n";
}

void test_spsc_high_performance() {
    std::cout << "=== SPSC High Performance Test ===\n";
    
    AtomicRingBuffer<int, 1024> buffer;
    constexpr int num_items = 100000;
    
    std::atomic<bool> start_flag{false};
    std::atomic<int> items_sent{0};
    std::atomic<int> items_received{0};
    
    // Single producer
    std::thread producer([&]() {
        // Wait for start signal
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
        // Wait for start signal
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
    
    std::cout << "Starting SPSC performance test with " << num_items << " items...\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double throughput = (num_items * 1000000.0) / duration.count();
    
    std::cout << "SPSC test completed:\n";
    std::cout << "  Time: " << duration.count() << " μs\n";
    std::cout << "  Throughput: " << static_cast<long>(throughput) << " items/sec\n";
    std::cout << "  Items sent: " << items_sent.load() << "\n";
    std::cout << "  Items received: " << items_received.load() << "\n\n";
}

void test_multiple_producers_consumers() {
    std::cout << "=== Multiple Producers/Consumers ===\n";
    
    AtomicRingBuffer<int, 256> buffer;
    constexpr int num_producers = 3;
    constexpr int num_consumers = 2;
    constexpr int items_per_producer = 200;
    
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};
    std::atomic<bool> production_done{false};
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    // Producer threads
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delay_dist(1, 10);
            
            for (int i = 0; i < items_per_producer; ++i) {
                int item = p * items_per_producer + i;
                
                while (!buffer.push(item)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                
                total_produced.fetch_add(1);
                
                // Variable delay
                std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(gen)));
            }
            
            std::cout << "Producer " << p << " finished\n";
        });
    }
    
    // Consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&, c]() {
            int my_consumed = 0;
            int value;
            
            while (!production_done.load() || !buffer.empty()) {
                if (buffer.pop(value)) {
                    my_consumed++;
                    total_consumed.fetch_add(1);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
            
            std::cout << "Consumer " << c << " finished. Consumed " << my_consumed << " items\n";
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
    
    std::cout << "Final results:\n";
    std::cout << "  Total produced: " << total_produced.load() << "\n";
    std::cout << "  Total consumed: " << total_consumed.load() << "\n";
    std::cout << "  Remaining in buffer: " << buffer.size() << "\n\n";
}

void test_emplace_operations() {
    std::cout << "=== Emplace Operations ===\n";
    
    AtomicRingBuffer<std::pair<int, std::string>, 16> buffer;
    
    // Test emplace with pair construction
    buffer.emplace(1, "first");
    buffer.emplace(2, "second");
    buffer.emplace(3, "third");
    
    std::cout << "Emplaced 3 pairs\n";
    std::cout << "Buffer size: " << buffer.size() << "\n";
    
    // Pop and verify
    std::pair<int, std::string> item;
    while (buffer.pop(item)) {
        std::cout << "  Popped: (" << item.first << ", " << item.second << ")\n";
    }
    
    std::cout << "\n";
}

void test_streaming_data_simulation() {
    std::cout << "=== Streaming Data Simulation ===\n";
    
    AtomicRingBuffer<double, 64> data_stream;
    constexpr int simulation_time_ms = 500;
    
    std::atomic<bool> keep_streaming{true};
    std::atomic<int> samples_generated{0};
    std::atomic<int> samples_processed{0};
    std::atomic<double> sum_processed{0.0};
    
    // Data generator (simulates sensor readings)
    std::thread generator([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> value_dist(0.0, 100.0);
        
        while (keep_streaming.load()) {
            double sample = value_dist(gen);
            
            if (data_stream.push(sample)) {
                samples_generated.fetch_add(1);
            }
            // else: buffer full, drop sample (realistic for streaming)
            
            std::this_thread::sleep_for(std::chrono::microseconds(1000)); // 1kHz
        }
    });
    
    // Data processor
    std::thread processor([&]() {
        double sample;
        double local_sum = 0.0;
        int local_count = 0;
        
        while (keep_streaming.load() || !data_stream.empty()) {
            if (data_stream.pop(sample)) {
                local_sum += sample;
                local_count++;
                samples_processed.fetch_add(1);
                
                // Simulate processing time
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        sum_processed.store(local_sum);
    });
    
    std::cout << "Running streaming simulation for " << simulation_time_ms << " ms...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(simulation_time_ms));
    
    keep_streaming.store(false);
    
    generator.join();
    processor.join();
    
    double avg_value = samples_processed.load() > 0 ? 
                      sum_processed.load() / samples_processed.load() : 0.0;
    
    std::cout << "Streaming simulation results:\n";
    std::cout << "  Samples generated: " << samples_generated.load() << "\n";
    std::cout << "  Samples processed: " << samples_processed.load() << "\n";
    std::cout << "  Processing rate: " << std::fixed << std::setprecision(1)
              << (100.0 * samples_processed.load()) / samples_generated.load() << "%\n";
    std::cout << "  Average value: " << std::setprecision(2) << avg_value << "\n";
    std::cout << "  Final buffer size: " << data_stream.size() << "\n\n";
}

int main() {
    std::cout << "Lock-free RingBuffer Example\n";
    std::cout << "============================\n\n";
    
    test_basic_ringbuffer_operations();
    test_ringbuffer_capacity_limits();
    test_producer_consumer_pattern();
    test_spsc_high_performance();
    test_multiple_producers_consumers();
    test_emplace_operations();
    test_streaming_data_simulation();
    
    std::cout << "All RingBuffer tests completed!\n";
    std::cout << "\nNote: This RingBuffer implementation provides both general\n";
    std::cout << "multi-producer/multi-consumer operations and optimized SPSC\n";
    std::cout << "(Single Producer Single Consumer) variants for maximum performance.\n";
    
    return 0;
}