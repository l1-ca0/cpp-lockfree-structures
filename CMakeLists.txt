cmake_minimum_required(VERSION 3.16)
project(LockFreeStructures VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -O3 -march=native)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -O3 -march=native)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /O2)
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Library (header-only, no source files needed)
add_library(lockfree_structures INTERFACE)

target_link_libraries(lockfree_structures INTERFACE Threads::Threads)

# Examples (alphabetical order)
add_executable(bloomfilter_example examples/bloomfilter_example.cpp)
target_link_libraries(bloomfilter_example lockfree_structures)

add_executable(hashmap_example examples/hashmap_example.cpp)
target_link_libraries(hashmap_example lockfree_structures)

add_executable(linkedlist_example examples/linkedlist_example.cpp)
target_link_libraries(linkedlist_example lockfree_structures)

add_executable(mpmc_queue_example examples/mpmc_queue_example.cpp)
target_link_libraries(mpmc_queue_example lockfree_structures)

add_executable(priority_queue_example examples/priority_queue_example.cpp)
target_link_libraries(priority_queue_example lockfree_structures)

add_executable(queue_example examples/queue_example.cpp)
target_link_libraries(queue_example lockfree_structures)

add_executable(rbtree_example examples/rbtree_example.cpp)
target_link_libraries(rbtree_example lockfree_structures)

add_executable(ringbuffer_example examples/ringbuffer_example.cpp)
target_link_libraries(ringbuffer_example lockfree_structures)

add_executable(set_example examples/set_example.cpp)
target_link_libraries(set_example lockfree_structures)

add_executable(skiplist_example examples/skiplist_example.cpp)
target_link_libraries(skiplist_example lockfree_structures)

add_executable(stack_example examples/stack_example.cpp)
target_link_libraries(stack_example lockfree_structures)

add_executable(trie_example examples/trie_example.cpp)
target_link_libraries(trie_example lockfree_structures)

add_executable(work_stealing_deque_example examples/work_stealing_deque_example.cpp)
target_link_libraries(work_stealing_deque_example lockfree_structures)

# Enable testing
enable_testing()

# Tests (alphabetical order)
add_executable(test_bloomfilter test/test_bloomfilter.cpp)
target_link_libraries(test_bloomfilter lockfree_structures)
add_test(NAME BloomFilterTests COMMAND test_bloomfilter)

add_executable(test_hashmap test/test_hashmap.cpp)
target_link_libraries(test_hashmap lockfree_structures)
add_test(NAME HashMapTests COMMAND test_hashmap)

add_executable(test_linkedlist test/test_linkedlist.cpp)
target_link_libraries(test_linkedlist lockfree_structures)
add_test(NAME LinkedListTests COMMAND test_linkedlist)

add_executable(test_mpmc_queue test/test_mpmc_queue.cpp)
target_link_libraries(test_mpmc_queue lockfree_structures)
add_test(NAME MPMCQueueTests COMMAND test_mpmc_queue)

add_executable(test_priority_queue test/test_priority_queue.cpp)
target_link_libraries(test_priority_queue lockfree_structures)
add_test(NAME PriorityQueueTests COMMAND test_priority_queue)

add_executable(test_queue test/test_queue.cpp)
target_link_libraries(test_queue lockfree_structures)
add_test(NAME QueueTests COMMAND test_queue)

add_executable(test_rbtree test/test_rbtree.cpp)
target_link_libraries(test_rbtree lockfree_structures)
add_test(NAME RBTreeTests COMMAND test_rbtree)

add_executable(test_ringbuffer test/test_ringbuffer.cpp)
target_link_libraries(test_ringbuffer lockfree_structures)
add_test(NAME RingBufferTests COMMAND test_ringbuffer)

add_executable(test_set test/test_set.cpp)
target_link_libraries(test_set lockfree_structures)
add_test(NAME SetTests COMMAND test_set)

add_executable(test_skiplist test/test_skiplist.cpp)
target_link_libraries(test_skiplist lockfree_structures)
add_test(NAME SkipListTests COMMAND test_skiplist)

add_executable(test_stack test/test_stack.cpp)
target_link_libraries(test_stack lockfree_structures)
add_test(NAME StackTests COMMAND test_stack)

add_executable(test_trie test/test_trie.cpp)
target_link_libraries(test_trie lockfree_structures)
add_test(NAME TrieTests COMMAND test_trie)

add_executable(test_work_stealing_deque test/test_work_stealing_deque.cpp)
target_link_libraries(test_work_stealing_deque lockfree_structures)
add_test(NAME WorkStealingDequeTests COMMAND test_work_stealing_deque)

# Benchmarks (alphabetical order)
add_executable(benchmark_bloomfilter benchmark/benchmark_bloomfilter.cpp)
target_link_libraries(benchmark_bloomfilter lockfree_structures)

add_executable(benchmark_hashmap benchmark/benchmark_hashmap.cpp)
target_link_libraries(benchmark_hashmap lockfree_structures)

add_executable(benchmark_linkedlist benchmark/benchmark_linkedlist.cpp)
target_link_libraries(benchmark_linkedlist lockfree_structures)

add_executable(benchmark_mpmc_queue benchmark/benchmark_mpmc_queue.cpp)
target_link_libraries(benchmark_mpmc_queue lockfree_structures)

add_executable(benchmark_priority_queue benchmark/benchmark_priority_queue.cpp)
target_link_libraries(benchmark_priority_queue lockfree_structures)

add_executable(benchmark_queue benchmark/benchmark_queue.cpp)
target_link_libraries(benchmark_queue lockfree_structures)

add_executable(benchmark_rbtree benchmark/benchmark_rbtree.cpp)
target_link_libraries(benchmark_rbtree lockfree_structures)

add_executable(benchmark_ringbuffer benchmark/benchmark_ringbuffer.cpp)
target_link_libraries(benchmark_ringbuffer lockfree_structures)

add_executable(benchmark_set benchmark/benchmark_set.cpp)
target_link_libraries(benchmark_set lockfree_structures)

add_executable(benchmark_skiplist benchmark/benchmark_skiplist.cpp)
target_link_libraries(benchmark_skiplist lockfree_structures)

add_executable(benchmark_stack benchmark/benchmark_stack.cpp)
target_link_libraries(benchmark_stack lockfree_structures)

add_executable(benchmark_trie benchmark/benchmark_trie.cpp)
target_link_libraries(benchmark_trie lockfree_structures)

add_executable(benchmark_work_stealing_deque benchmark/benchmark_work_stealing_deque.cpp)
target_link_libraries(benchmark_work_stealing_deque lockfree_structures)