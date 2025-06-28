#include <lockfree/atomic_work_stealing_deque.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <cassert>
#include <random>
#include <chrono>


struct TestItem {
    int value;
    explicit TestItem(int v) : value(v) {}
    TestItem(const TestItem&) = default;
    TestItem(TestItem&&) = default;
    TestItem& operator=(const TestItem&) = default;
    TestItem& operator=(TestItem&&) = default;
};

void test_basic_operations() {
    std::cout << "Testing basic operations...\n";
    
    lockfree::AtomicWorkStealingDeque<TestItem> deque;
    
    // Test empty deque
    assert(deque.empty());
    assert(deque.size() == 0);
    assert(deque.pop_bottom() == nullptr);
    assert(deque.steal() == nullptr);
    
    // Test push_bottom and pop_bottom (LIFO)
    deque.push_bottom(TestItem(1));
    deque.push_bottom(TestItem(2));
    deque.push_bottom(TestItem(3));
    
    assert(!deque.empty());
    assert(deque.size() == 3);
    
    // Pop should return in LIFO order
    TestItem* item = deque.pop_bottom();
    assert(item && item->value == 3);
    delete item;
    
    item = deque.pop_bottom();
    assert(item && item->value == 2);
    delete item;
    
    item = deque.pop_bottom();
    assert(item && item->value == 1);
    delete item;
    
    assert(deque.empty());
    assert(deque.pop_bottom() == nullptr);
    
    std::cout << "✓ Basic operations test passed\n";
}

void test_steal_operations() {
    std::cout << "Testing steal operations...\n";
    
    lockfree::AtomicWorkStealingDeque<TestItem> deque;
    
    // Push elements
    deque.push_bottom(TestItem(1));
    deque.push_bottom(TestItem(2));
    deque.push_bottom(TestItem(3));
    
    // Steal should return in FIFO order (oldest first)
    TestItem* item = deque.steal();
    assert(item && item->value == 1);
    delete item;
    
    item = deque.steal();
    assert(item && item->value == 2);
    delete item;
    
    item = deque.steal();
    assert(item && item->value == 3);
    delete item;
    
    assert(deque.empty());
    assert(deque.steal() == nullptr);
    
    std::cout << "✓ Steal operations test passed\n";
}

void test_mixed_operations() {
    std::cout << "Testing mixed push/pop/steal operations...\n";
    
    lockfree::AtomicWorkStealingDeque<TestItem> deque;
    
    // Mixed scenario: push, steal some, pop some
    deque.push_bottom(TestItem(1));
    deque.push_bottom(TestItem(2));
    deque.push_bottom(TestItem(3));
    deque.push_bottom(TestItem(4));
    
    // Steal oldest (1)
    TestItem* item = deque.steal();
    assert(item && item->value == 1);
    delete item;
    
    // Pop newest (4)
    item = deque.pop_bottom();
    assert(item && item->value == 4);
    delete item;
    
    // Add more
    deque.push_bottom(TestItem(5));
    
    // Steal next oldest (2)
    item = deque.steal();
    assert(item && item->value == 2);
    delete item;
    
    // Remaining should be 3, 5 (5 is newest)
    item = deque.pop_bottom();
    assert(item && item->value == 5);
    delete item;
    
    item = deque.pop_bottom();
    assert(item && item->value == 3);
    delete item;
    
    assert(deque.empty());
    
    std::cout << "✓ Mixed operations test passed\n";
}

void test_single_element_race() {
    std::cout << "Testing single element race condition...\n";
    
    lockfree::AtomicWorkStealingDeque<TestItem> deque;
    std::atomic<int> results{0};
    std::atomic<bool> start{false};
    
    // Owner thread
    std::thread owner([&]() {
        while (!start.load()) std::this_thread::yield();
        
        for (int i = 0; i < 1000; ++i) {
            deque.push_bottom(TestItem(i));
            
            // Try to pop immediately
            TestItem* item = deque.pop_bottom();
            if (item) {
                results.fetch_add(item->value);
                delete item;
            }
        }
    });
    
    // Thief thread
    std::thread thief([&]() {
        while (!start.load()) std::this_thread::yield();
        
        for (int i = 0; i < 1000; ++i) {
            TestItem* item = deque.steal();
            if (item) {
                results.fetch_add(item->value);
                delete item;
            }
            std::this_thread::yield();
        }
    });
    
    start.store(true);
    owner.join();
    thief.join();
    
    // All elements should be processed exactly once
    // Sum of 0 to 999 = 999 * 1000 / 2 = 499500
    int expected_sum = 999 * 1000 / 2;
    assert(results.load() == expected_sum);
    
    std::cout << "✓ Single element race test passed\n";
}

void test_concurrent_operations() {
    std::cout << "Testing concurrent operations...\n";
    
    lockfree::AtomicWorkStealingDeque<TestItem> deque;
    const int num_items = 4000;  // Reduced to stay within capacity
    const int num_thieves = 4;
    
    std::atomic<int> items_pushed{0};
    std::atomic<int> items_popped_by_owner{0};
    std::atomic<int> items_stolen{0};
    std::set<int> processed_items;
    std::mutex processed_mutex;
    
    // Owner thread - pushes and occasionally pops
    std::thread owner([&]() {
        for (int i = 0; i < num_items; ++i) {
            deque.push_bottom(TestItem(i));
            items_pushed.fetch_add(1);
            
            // Occasionally pop own items
            if (i % 10 == 0) {
                TestItem* item = deque.pop_bottom();
                if (item) {
                    {
                        std::lock_guard<std::mutex> lock(processed_mutex);
                        processed_items.insert(item->value);
                    }
                    delete item;
                    items_popped_by_owner.fetch_add(1);
                }
            }
        }
        
        // Process remaining items
        TestItem* item;
        while ((item = deque.pop_bottom()) != nullptr) {
            {
                std::lock_guard<std::mutex> lock(processed_mutex);
                processed_items.insert(item->value);
            }
            delete item;
            items_popped_by_owner.fetch_add(1);
        }
    });
    
    // Thief threads
    std::vector<std::thread> thieves;
    std::atomic<bool> stop_thieves{false};
    
    for (int t = 0; t < num_thieves; ++t) {
        thieves.emplace_back([&]() {
            while (!stop_thieves.load()) {
                TestItem* item = deque.steal();
                if (item) {
                    {
                        std::lock_guard<std::mutex> lock(processed_mutex);
                        processed_items.insert(item->value);
                    }
                    delete item;
                    items_stolen.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    owner.join();
    
    // Wait a bit for thieves to finish stealing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_thieves.store(true);
    
    for (auto& thief : thieves) {
        thief.join();
    }
    
    // Verify all items were processed exactly once
    assert(processed_items.size() == num_items);
    for (int i = 0; i < num_items; ++i) {
        assert(processed_items.count(i) == 1);
    }
    
    int total_processed = items_popped_by_owner.load() + items_stolen.load();
    assert(total_processed == num_items);
    
    std::cout << "✓ Concurrent operations test passed\n";
    std::cout << "  Items processed by owner: " << items_popped_by_owner.load() << "\n";
    std::cout << "  Items stolen by thieves: " << items_stolen.load() << "\n";
}

void test_capacity_functionality() {
    std::cout << "Testing capacity functionality...\n";
    
    lockfree::AtomicWorkStealingDeque<TestItem> deque;
    
    // Push items up to near capacity
    const int many_items = 1000;
    for (int i = 0; i < many_items; ++i) {
        deque.push_bottom(TestItem(i));
    }
    
    assert(deque.size() == many_items);
    
    // Verify all items are present and in correct order
    for (int i = many_items - 1; i >= 0; --i) {
        TestItem* item = deque.pop_bottom();
        assert(item && item->value == i);
        delete item;
    }
    
    assert(deque.empty());
    
    std::cout << "✓ Capacity functionality test passed\n";
}

void test_memory_management() {
    std::cout << "Testing memory management...\n";
    
    // Test that destructor properly cleans up
    {
        lockfree::AtomicWorkStealingDeque<TestItem> deque;
        
        for (int i = 0; i < 100; ++i) {
            deque.push_bottom(TestItem(i));
        }
        
        // Pop some but not all
        for (int i = 0; i < 50; ++i) {
            TestItem* item = deque.pop_bottom();
            delete item;
        }
        
        // Deque destructor should clean up remaining items
    }
    
    std::cout << "✓ Memory management test passed\n";
}

int main() {
    std::cout << "Work-Stealing Deque Test Suite\n";
    std::cout << "==============================\n\n";
    
    try {
        test_basic_operations();
        test_steal_operations();
        test_mixed_operations();
        test_single_element_race();
        test_concurrent_operations();
        test_capacity_functionality();
        test_memory_management();
        
        std::cout << "\n🎉 All tests passed successfully!\n";
        std::cout << "The work-stealing deque implementation is working correctly.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception\n";
        return 1;
    }
    
    return 0;
} 