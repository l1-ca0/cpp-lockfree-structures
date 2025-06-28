# Modern C++ Lock-Free Data Structures

[![C++](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-brightgreen.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com/)

A comprehensive library of lock-free concurrent data structures implemented using modern C++17/20 features. This library provides high-performance, thread-safe data structures without using traditional locking mechanisms.

## Table of Contents

**Getting Started**
- [Features](#-features)
- [Quick Start](#-quick-start)
- [Performance Guidelines](#-performance-guidelines)

**Choosing Data Structures**
- [Data Structure Selection Guide](#-data-structure-selection-guide)
- [Performance Characteristics](#-performance-characteristics)
- [Real-World Usage Patterns](#-real-world-usage-patterns)

**Implementation Details**
- [Building the Library](#-building-the-library)
- [Thread Safety Guarantees](#-thread-safety-guarantees)
- [Compiler & Platform Notes](#-compiler--platform-notes)

**Performance & Optimization**
- [Performance Benchmarks](#-performance-benchmarks)
- [Performance Analysis Summary](#-performance-analysis-summary)
- [Troubleshooting & Debugging](#-troubleshooting--debugging)

**Reference**
- [Library Structure](#-library-structure)
- [Design Decisions](#-design-decisions)
- [Limitations and Future Work](#-limitations-and-future-work)
- [Common Pitfalls](#-common-pitfalls)
- [Contributing](#-contributing)
- [License](#-license)
- [References](#-references)

## 🛠️ Features

**🔓 Lock-Free Programming**
Lock-free programming eliminates traditional locking mechanisms (mutexes, semaphores) and instead uses atomic operations and careful memory ordering to ensure thread safety. This approach prevents lock-based thread blocking, eliminates lock-induced deadlocks, and guarantees that at least one thread makes progress even under high contention.

**🚀 Comprehensive Lock-Free Library**
A collection of concurrent data structures built on battle-tested algorithms including Michael & Scott Queue, Chase-Lev Work-Stealing Deque, and Treiber Stack—most delivering lock-free thread safety with minimal blocking.

**⚡ Optimized for Concurrency**
- **Multi-producer/multi-consumer** support with SPSC optimizations where beneficial
- **Work-stealing patterns** with dedicated deque implementation
- **Bounded retry logic** prevents infinite loops under contention
- **Progressive backoff strategies** for optimal performance under contention

**🛠️ Modern C++ Design**
- **Header-only** library with C++17/20 features
- **Template-based** with zero-cost abstractions
- **Exception-safe** with RAII principles and proper cleanup
- **CPU-specific optimizations** for x86/ARM architectures

## 🚀 Quick Start

### High-Performance Hash Map
```cpp
#include "lockfree/atomic_hashmap.hpp"

lockfree::AtomicHashMap<std::string, int> cache;

// Multiple threads can safely access
cache.insert("key1", 100);
cache.insert("key2", 200);

int value;
if (cache.find("key1", value)) {
    std::cout << "Found: " << value << std::endl;
}
```

### Complete Producer-Consumer Example
```cpp
#include "lockfree/atomic_mpmc_queue.hpp"
#include <thread>
#include <iostream>
#include <atomic>

int main() {
    lockfree::AtomicMPMCQueue<int, 1024> queue;
    std::atomic<bool> done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            while (!queue.enqueue(i)) {
                std::this_thread::yield(); // Handle bounded capacity
            }
        }
        done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        int count = 0;
        while (!done || !queue.empty()) {
            if (queue.dequeue(value)) {
                std::cout << "Processed: " << value << std::endl;
                ++count;
            }
        }
        std::cout << "Total processed: " << count << std::endl;
    });
    
    producer.join();
    consumer.join();
    return 0;
}
```

## ⚡ Performance Guidelines

### When Lock-Free Excels
- **2+ concurrent threads** with significant contention
- **High-frequency operations** (>10K ops/sec per thread)
- **Real-time systems** requiring predictable latency
- **Producer-consumer** patterns with different thread counts
- **Read-heavy workloads** with occasional writes
- **Cache-friendly access patterns** with good locality

### When Mutex May Be Better
- **Single-threaded** or very low contention (<2 threads)
- **Complex multi-step operations** requiring multiple data structure accesses
- **Memory-constrained** environments (lock-free uses more memory)
- **Rapid prototyping** where development speed matters more than performance
- **Write-heavy workloads** with frequent structural changes
- **Debugging phase** (easier to reason about sequential code)

### Performance Expectations
- **2-15x speedup** possible under high contention
- **Memory overhead** of 20-50% compared to mutex versions
- **Best results** with compiler optimizations (`-O3`) enabled

## 📋 Data Structure Selection Guide

| **Use Case** | **Recommended Structure** | **Why** |
|--------------|---------------------------|---------|
| **LIFO operations** | `AtomicStack` | Simple, fast, Treiber algorithm |
| **FIFO message passing** | `AtomicQueue` | Michael & Scott, proven reliability |
| **High-contention MPMC** | `AtomicMPMCQueue` | Optimized for multiple producers/consumers |
| **Insertion-ordered iteration** | `AtomicLinkedList` | Maintains order, allows mid-list insertion/removal |
| **Ordered key-value storage** | `AtomicRBTree` | Self-balancing, O(log n) guaranteed |
| **Fast membership testing** | `AtomicBloomFilter` | Space-efficient, probabilistic |
| **Task distribution** | `AtomicWorkStealingDeque` | Optimized for work-stealing patterns |
| **Range queries, ordered data** | `AtomicSkipList` | Probabilistic O(log n), good for ranges |
| **Bounded buffering** | `AtomicRingBuffer` | Fixed memory, SPSC optimized |
| **String prefix matching** | `AtomicTrie` | Prefix operations, autocomplete |
| **Fast key-value lookup** | `AtomicHashMap` | O(1) average, hash-based |
| **Unique elements** | `AtomicSet` | Hash-based deduplication, O(1) average |
| **Priority-based processing** | `AtomicPriorityQueue` | Lock-free skip list based priority ordering |

## 📊 Performance Characteristics

| **Data Structure** | **Insert/Push** | **Remove/Pop** | **Access/Find** | **Space** | **Special Notes** |
|-------------------|-----------------|----------------|-----------------|-----------|-------------------|
| **AtomicStack<T>** | O(1) | O(1) | O(1) peek | O(n) | LIFO ordering, O(n) size() |
| **AtomicQueue<T>** | O(1) | O(1) | O(1) peek | O(n) | FIFO ordering, O(n) size() |
| **AtomicMPMCQueue<T,Size>** | O(1) | O(1) | O(1) front | O(Size) | MPMC optimized, bounded capacity |
| **AtomicWorkStealingDeque<T>** | O(1) push_bottom | O(1) pop_bottom/steal | - | O(4096) | Fixed capacity, owner/thief access |
| **AtomicPriorityQueue<T>** | O(log n) | O(log n) | O(1) top | O(n) | Lock-free skip list based priority ordering, O(n) size() |
| **AtomicRBTree<K,V>** | O(log n) | O(log n) | O(log n) | O(n) | Self-balancing, ordered |
| **AtomicHashMap<K,V>** | O(1) avg, O(n) worst | O(1) avg, O(n) worst | O(1) avg, O(n) worst | O(n) | Hash collisions affect worst case |
| **AtomicRingBuffer<T,Size>** | O(1) | O(1) | O(1) front/back | O(Size) | Template-sized, bounded capacity |
| **AtomicLinkedList<T>** | O(n) | O(n) | O(n) | O(n) | Linear search required |
| **AtomicSkipList<K,V>** | O(log n) expected | O(log n) expected | O(log n) expected | O(n) | Probabilistic performance, O(n) size() |
| **AtomicSet<T>** | O(1) avg, O(n) worst | O(1) avg, O(n) worst | O(1) avg, O(n) worst | O(n) | Hash-based, unique elements |
| **AtomicTrie<CharType>** | O(k) | O(k) | O(k) find, O(k+m) prefix | O(ALPHABET × n × k) | k = key length, prefix operations |
| **AtomicBloomFilter<T>** | O(k) | - | O(k) contains | O(m) bits | k = hash functions, probabilistic membership |

### **Performance Legend:**
- **n** = number of elements, **k** = key/hash length, **m** = filter size
- **avg/worst** = average/worst case, **expected** = probabilistic performance

## 🌍 Real-World Usage Patterns

### Web Server Request Queue
```cpp
#include "lockfree/atomic_mpmc_queue.hpp"

// High-throughput web server using lock-free request queue
class WebServer {
    lockfree::AtomicMPMCQueue<HttpRequest, 8192> request_queue_;
    std::vector<std::thread> worker_threads_;
    
public:
    void start(int worker_count) {
        for (int i = 0; i < worker_count; ++i) {
            worker_threads_.emplace_back([this]() {
                HttpRequest request;
                while (running_) {
                    if (request_queue_.dequeue(request)) {
                        process_request(request);
                    }
                }
            });
        }
    }
    
    void enqueue_request(const HttpRequest& request) {
        while (!request_queue_.enqueue(request)) {
            // Handle backpressure - could implement rate limiting here
            std::this_thread::yield();
        }
    }
};
```

### Game Engine Task System
```cpp
#include "lockfree/atomic_work_stealing_deque.hpp"

// Per-thread work stealing for game engine parallelism
class TaskSystem {
    std::vector<lockfree::AtomicWorkStealingDeque<Task*>> per_thread_deques_;
    std::vector<std::thread> worker_threads_;
    
public:
    void schedule_task(Task* task) {
        int thread_id = get_current_thread_id();
        per_thread_deques_[thread_id].push_bottom(task);
    }
    
    void work_stealing_loop(int thread_id) {
        auto& my_deque = per_thread_deques_[thread_id];
        
        while (running_) {
            Task* task = my_deque.pop_bottom();
            if (!task) {
                // Try stealing from other threads
                for (int i = 0; i < per_thread_deques_.size(); ++i) {
                    if (i != thread_id) {
                        task = per_thread_deques_[i].steal();
                        if (task) break;
                    }
                }
            }
            
            if (task) {
                task->execute();
                delete task;
            }
        }
    }
};
```

### High-Frequency Trading Cache
```cpp
#include "lockfree/atomic_hashmap.hpp"
#include "lockfree/atomic_priority_queue.hpp"

// Real-time market data cache with price prioritization
class MarketDataCache {
    lockfree::AtomicHashMap<std::string, MarketData> symbol_cache_;
    lockfree::AtomicPriorityQueue<PriceUpdate> priority_updates_;
    
public:
    void update_price(const std::string& symbol, double price, uint64_t timestamp) {
        MarketData data{price, timestamp};
        symbol_cache_.insert_or_update(symbol, data);
        
        // High-priority updates get processed first
        PriceUpdate update{symbol, price, timestamp};
        priority_updates_.push(update);
    }
    
    bool get_latest_price(const std::string& symbol, double& price) {
        MarketData data;
        if (symbol_cache_.find(symbol, data)) {
            price = data.price;
            return true;
        }
        return false;
    }
    
    void process_priority_updates() {
        PriceUpdate update;
        while (priority_updates_.pop(update)) {
            notify_subscribers(update);
        }
    }
};
```

### Event Processing Pipeline
```cpp
#include "lockfree/atomic_queue.hpp"
#include "lockfree/atomic_bloomfilter.hpp"

// Event deduplication and processing pipeline
class EventProcessor {
    lockfree::AtomicQueue<Event> event_queue_;
    lockfree::AtomicBloomFilter<std::string> seen_events_;
    
public:
    bool submit_event(const Event& event) {
        // Fast deduplication check
        if (seen_events_.contains(event.id)) {
            return false; // Duplicate event
        }
        
        seen_events_.insert(event.id);
        event_queue_.enqueue(event);
        return true;
    }
    
    void process_events() {
        Event event;
        while (event_queue_.dequeue(event)) {
            handle_event(event);
        }
    }
};
```

## 🏗️ Building the Library

### Prerequisites

**Compilers:**
- **GCC 8+** (for designated initializers; GCC 10+ recommended for full C++20)
- **Clang 10+** (Clang 12+ recommended for complete C++20 support)  
- **MSVC 2019 16.8+** (Visual Studio 2019/2022)

**Platforms:**
- **Linux** (Ubuntu 20.04+, CentOS 8+, Fedora 32+)
- **macOS** (10.15+, both Intel and Apple Silicon)
- **Windows** (Windows 10+, WSL2 supported)

**Build Tools:**
- **CMake 3.16+** (3.20+ recommended)
- **Make** or **Ninja** build system
- **Threads library** (pthread on Unix, Win32 threads on Windows)

### Quick Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run Examples
```bash
make stack_example && ./stack_example
make queue_example && ./queue_example
```

### Run Tests
```bash
ctest --verbose
```

## 🔒 Thread Safety Guarantees

- **Multi-threaded Design**: Structures designed for concurrent access with specific usage patterns
- **Atomic Operations**: Uses atomic primitives and memory ordering to coordinate concurrent access
- **Resource Cleanup**: RAII cleanup prevents most leaks, though logical deletion may retain some memory longer
- **Retry Limits**: Contended operations use bounded retry loops to avoid infinite spinning
- **Best-Effort ABA Protection**: Practical patterns reduce common race conditions 

## 🔧 Compiler & Platform Notes

### Recommended Compiler Flags
```bash
# For maximum performance
g++ -O3 -march=native -DNDEBUG -pthread

# For debugging
g++ -O0 -g -fsanitize=thread -fsanitize=address -pthread

# For profiling
g++ -O3 -g -fno-omit-frame-pointer -pthread
```

### Platform-Specific Considerations
- **x86/x64**: Full atomic support, pause instructions optimized for Intel/AMD
- **ARM64**: Native atomic support, yield instructions for Apple Silicon and ARM servers
- **macOS**: Excellent threading support, good performance across Intel and M1/M2
- **Linux**: Best performance, widest testing, optimal for server deployments
- **Windows**: Good support via MSVC 2019+, WSL2 recommended for development

### Memory Model Notes
- All structures use **C++11 memory model** with appropriate memory ordering
- **Sequential consistency** used sparingly for maximum performance
- **Acquire-release semantics** for most pointer operations
- **Relaxed ordering** for counters and non-critical metadata

## 🚀 Performance Benchmarks

## 📊 Performance at a Glance

Thread Scaling (relative to mutex implementation using 16 threads):
```
RingBuffer    ████████████████ 18.56x (SPSC, 1 thread)
Set           ████████████████ 14.56x
RBTree        ████████████████ 14.48x
HashMap       ████████████████ 13.16x  
LinkedList    ██████████       9.90x
MPMCQueue     █████████        8.94x
PriorityQueue ███████          6.87x
WorkDeque     █████            4.73x
BloomFilter   ████             3.95x
Trie          ███              2.84x
Stack         ██               1.49x
SkipList      ██               1.46x
Queue         ██               1.41x
```

## 🚀 **Performance Analysis Summary**

*Benchmarks conducted on 8-core system with -O3 optimization across 1-16 threads, testing read-heavy, write-heavy, and balanced workloads under high contention.*

| Structure | Key Strength & Best Use Case | 2 Threads | 8 Threads | 16 Threads |
|-----------|-------------------------------|-----------|-----------|------------|
| **RingBuffer** | SPSC optimization (bursty traffic) | **18.56x** | N/A* | N/A* |
| **Set** | Deduplication of unique elements | **5.39x** | **14.14x** | **14.56x** |
| **RBTree** | Outstanding all-around performance | **4.55x** | **15.33x** | **14.48x** |
| **HashMap** | High contention hash lookups | **5.99x** | **15.84x** | **13.16x** |
| **LinkedList** | Excellent scaling for ordered insertion | **1.78x** | **6.97x** | **9.90x** |
| **MPMCQueue** | Multi-producer/consumer under high contention | **6.08x** | **9.65x** | **8.94x** |
| **PriorityQueue** | Lock-free priority processing | **1.96x** | **6.40x** | **6.87x** |
| **WorkStealingDeque** | Work stealing for task distribution | **6.66x** | **3.53x** | **4.73x** |
| **BloomFilter** | High contention membership testing | **2.07x** | **4.02x** | **3.95x** |
| **Trie** | String prefix matching operations | 0.85x | **2.82x** | **2.84x** |
| **Stack** | Scalable LIFO operations | **1.04x** | **1.14x** | **1.49x** |
| **SkipList** | Probabilistic ordered operations | **1.14x** | **2.36x** | **1.46x** |
| **Queue** | Producer-consumer message passing | **1.29x** | **1.47x** | **1.41x** |

*Results from averaging 5 benchmark runs. Performance gains are lock-free vs mutex implementations. Results may vary by hardware and workload.*

*RingBuffer is designed for SPSC (Single Producer Single Consumer) scenarios, so multi-thread comparisons are not applicable.*

## 🔧 Troubleshooting & Debugging

### Performance Issues
- **Profile first**: Use tools like `perf`, `vtune`, or `instruments`
- **Check thread count**: More threads ≠ better performance always
- **Monitor cache misses**: Lock-free structures can be cache-intensive
- **Verify compiler optimizations**: Always benchmark with `-O3`
- **Watch for false sharing**: Ensure proper alignment and spacing
- **Check memory ordering**: Overly strong ordering can hurt performance

### Memory Issues  
- **Watch for leaks**: Use AddressSanitizer (`-fsanitize=address`)
- **Check alignment**: Ensure proper cache line alignment
- **Monitor growth**: Some structures don't shrink automatically
- **Logical deletion**: Marked nodes may persist longer than expected
- **Stack overflow**: Deep recursion in tree traversals

### Common Performance Pitfalls
- **Size() calls in hot paths**: Many structures have O(n) size()
- **Mixing paradigms**: Don't mix lock-free with mutex-based code
- **Capacity limits**: Check bounds for fixed-size structures
- **Retry storms**: High contention can cause excessive retries
- **Memory allocation**: Frequent new/delete can become bottleneck

### Debugging Tips
```bash
# Thread sanitizer for race conditions
g++ -fsanitize=thread -O2 -g

# Address sanitizer for memory errors
g++ -fsanitize=address -O1 -g

# Debug symbols with optimizations
g++ -O3 -g -fno-omit-frame-pointer

# Valgrind for memory analysis
valgrind --tool=helgrind ./your_program
```

### Common Error Patterns
- **ABA Problem**: Pointer reuse causing consistency issues
- **Memory Ordering**: Insufficient synchronization between threads
- **Capacity Overflow**: Bounded structures rejecting operations
- **Logical Deletion**: Marked nodes affecting traversal logic

## 📁 Library Structure

### 📁 Core Data Structures (`include/lockfree/`)

| **Category** | **Files** | **Purpose** |
|--------------|-----------|-------------|
| **Linear** | `atomic_stack.hpp`, `atomic_queue.hpp`, `atomic_mpmc_queue.hpp`, `atomic_linkedlist.hpp` | LIFO/FIFO operations, MPMC patterns, ordered insertion |
| **Specialized** | `atomic_work_stealing_deque.hpp`, `atomic_ringbuffer.hpp`, `atomic_priority_queue.hpp` | Task distribution, bounded buffers, priority processing |
| **Tree/Ordered** | `atomic_rbtree.hpp`, `atomic_skiplist.hpp` | Key-value storage, range queries |
| **Hash-Based** | `atomic_hashmap.hpp`, `atomic_set.hpp` | Fast lookup, unique elements |
| **Algorithms** | `atomic_trie.hpp`, `atomic_bloomfilter.hpp` | String operations, membership testing |

### 📁 Supporting Files

| **Directory** | **Contents** | **Quick Commands** |
|---------------|--------------|-------------------|
| **`examples/`** | Usage demonstrations for each structure | `make stack_example && ./stack_example` |
| **`benchmark/`** | Performance analysis vs mutex implementations | `find . -name "benchmark_*" -exec {} \;` |
| **`test/`** | Correctness verification and stress testing | `ctest --verbose` |

## ⚙️ Design Decisions

### Memory Management

**Approach**: Simple and predictable memory handling optimized for performance.
- **Stack/Queue**: Immediate deallocation after successful operations, O(n) size() for performance
- **Trees/Lists**: Logical deletion (mark deleted, clean up during traversal)
- **All structures**: Automatic cleanup in destructors

### Memory Ordering Strategy

**Philosophy**: Minimize the synchronization cost while ensuring correctness.

**Ordering Selection**:
- **`memory_order_relaxed`**: Counters, statistics, and non-critical metadata
- **`memory_order_acquire`**: Loading pointers that need to see preceding writes
- **`memory_order_release`**: Publishing data/pointers to other threads
- **`memory_order_acq_rel`**: Read-modify-write operations needing both semantics
- **`memory_order_seq_cst`**: Only when linearization points require total ordering

**Performance Impact**: Reduces memory fence overhead compared to seq_cst everywhere by using weaker orderings where safe.

### Bounded Retry Logic

**Purpose**: Prevents infinite loops under extreme contention.
- **1000 attempt limit** for contended operations
- **Graceful failure** rather than blocking
- **Progressive backoff strategy**: CPU pause → progressive delay → thread yield

### Fixed vs Dynamic Capacity

**Decisions per Structure**:
- **Fixed**: WorkStealingDeque (4096 elements), RingBuffer (template-specified size)
- **Unbounded**: Stack, Queue, RBTree (grow as needed via node allocation)
- **Fixed-size buckets**: HashMap, Set (fixed bucket count, no resizing implemented)
- **Fixed bit array**: BloomFilter (size set at construction, configurable)

**Rationale**: Lock-free resizing of hash tables is significantly more complex than node-based growth, so HashMap/Set use fixed bucket arrays for reliability.

## ⚠️ Limitations and Future Work

### Current Limitations

**📏 Fixed Capacity Constraints**
- **WorkStealingDeque** limited to 4096 elements (avoids resize complexity)
- **RingBuffer** requires compile-time size specification
- **HashMap/Set** use fixed bucket arrays (no resizing implemented)
- **BloomFilter** bit array size fixed at construction

**💾 Memory Characteristics** 
- **Logical deletion** can hold memory longer than immediate deallocation
- **No advanced reclamation** - uses simple immediate/logical deletion instead of hazard pointers or epoch-based schemes
- **Potential memory leaks** under extreme contention due to lack of advanced reclamation schemes

**🔄 Retry Behavior**
- **Operations may fail** after 1000 retry attempts under extreme contention
- **Progressive backoff implemented** but not fully adaptive to dynamic contention levels

**🎯 Use Case Specificity**
- **Performance varies significantly** by workload pattern and thread count
- **Not always faster** than mutex-based solutions for low-contention scenarios
- **Memory ordering** optimized for common cases, may not be optimal for all access patterns

### Future Enhancements

**🚀 Advanced Data Structures**
- Lock-free resizable hash tables and dynamic arrays
- NUMA-aware data structure variants
- Advanced skip list implementations with better probabilistic guarantees

**⚡ Performance Optimizations**
- Enhanced platform-specific atomic operation optimizations
- Fully adaptive retry strategies based on dynamic contention measurement
- Memory prefetching and cache-line optimization

**🔧 Memory Management**
- Hazard pointer or epoch-based memory reclamation
- Configurable memory allocation strategies
- Better support for custom allocators

**📊 Tooling and Analysis**
- Contention analysis and profiling tools
- Comprehensive benchmarking suite across more platforms
- Performance regression testing framework

## ❌ Common Pitfalls

❌ **Don't assume faster in all cases** - Profile first, especially for low contention scenarios  
❌ **Don't ignore capacity limits** - WorkStealingDeque (4096), RingBuffer (fixed size), HashMap/Set (fixed buckets)  
❌ **Don't forget cleanup** - Delete pointers returned from WorkStealingDeque::steal() and pop_bottom()  
❌ **Don't expect all operations to succeed** - Operations may fail after 1000 retry attempts under high contention  
❌ **Don't mix with regular STL** - Use consistent locking strategy across your codebase  
❌ **Don't call size() in hot paths** - Many structures have O(n) size() for better performance  
❌ **Don't ignore compiler optimizations** - Always benchmark with `-O3` enabled  
❌ **Don't assume linear scaling** - Performance plateaus after optimal thread count  

## 🤝 Contributing

- Feel free to open an issue to discuss ideas
- Please follow the existing code style
- Adding tests for new features helps ensure reliability
- Updating documentation helps other users
- Running the existing tests ensures nothing breaks

**Thank you** for considering contributing to this project! Every contribution is valued and appreciated. 🙏

## 📄 License

This project is licensed under the [MIT License](LICENSE). See the [LICENSE](LICENSE) file for the full license text.

## 📚 References

- **Michael, M. M., & Scott, M. L.** (1996). Simple, fast, and practical non-blocking and blocking concurrent queue algorithms. *Proceedings of the 15th ACM Symposium on Principles of Distributed Computing (PODC)*, 267-275. [DOI: 10.1145/248052.248106](https://doi.org/10.1145/248052.248106) *(Queue implementation)*

- **Lamport, L.** (1983). Specifying concurrent program modules. *ACM Transactions on Programming Languages and Systems*, 5(2), 190-222. [DOI: 10.1145/69575.69577](https://doi.org/10.1145/69575.69577) *(MPMC queue foundations and wait-free algorithms)*

- **Herlihy, M., & Shavit, N.** (2012). *The Art of Multiprocessor Programming, Revised Edition*. Morgan Kaufmann. ISBN: 978-0123973375 *(MPMC queue algorithms, bounded concurrent data structures)*

- **Chase, D., & Lev, Y.** (2005). Dynamic circular work-stealing deque. *Proceedings of the 17th ACM Symposium on Parallelism in Algorithms and Architectures (SPAA)*, 21-28. [DOI: 10.1145/1073970.1073974](https://doi.org/10.1145/1073970.1073974) *(Work-stealing deque implementation)*

- **Treiber, R. K.** (1986). Systems programming: Coping with parallelism. *Technical Report RJ 5118, IBM Almaden Research Center*. *(Stack implementation)*

- **Pugh, W.** (1990). Skip lists: A probabilistic alternative to balanced trees. *Communications of the ACM*, 33(6), 668-676. [DOI: 10.1145/78973.78977](https://doi.org/10.1145/78973.78977) *(Skip list implementation and lock-free priority queue)*

- **Bloom, B. H.** (1970). Space/time trade-offs in hash coding with allowable errors. *Communications of the ACM*, 13(7), 422-426. [DOI: 10.1145/362686.362692](https://doi.org/10.1145/362686.362692) *(Bloom filter implementation)*

- **Boehm, H.-J.** (2005). Threads cannot be implemented as a library. *Proceedings of the 2005 ACM SIGPLAN Conference on Programming Language Design and Implementation (PLDI)*, 261-268. [DOI: 10.1145/1065010.1065042](https://doi.org/10.1145/1065010.1065042) *(Memory ordering and atomic operations)*

- **Intel Corporation** (2021). *Intel® 64 and IA-32 Architectures Software Developer's Manual, Volume 3A: System Programming Guide*. *(CPU pause instructions and x86/x64 optimization techniques)*

- **ARM Limited** (2020). *ARM® Architecture Reference Manual ARMv8, for ARMv8-A architecture profile*. *(ARM-specific atomic operations and yield instructions)*

- **Anthony Williams** (2019). *C++ Concurrency in Action, Second Edition: Practical Multithreading*. Manning Publications. ISBN: 978-1617294693 *(Modern C++ concurrent programming practices and progressive backoff strategies)*
