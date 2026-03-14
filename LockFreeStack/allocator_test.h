#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <array>
#include <cassert>
#include "Allocator.h"

// ============================================================
// ТЕСТ 1: Базовый LIFO порядок

static void alloc_test_basic() {
    std::cout << "[1] Basic alloc/dealloc... ";

    Allocator<int> alloc(0);

    Node n1 = alloc.allocate(10);
    Node n2 = alloc.allocate(20);
    Node n3 = alloc.allocate(30);

    assert(alloc.deallocate(n3) == 30);
    assert(alloc.deallocate(n2) == 20);
    assert(alloc.deallocate(n1) == 10);

    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 2: Заполнение одной таблицы (128 элементов)
static void alloc_test_fill_table() {
    std::cout << "[2] Fill one table (128 items)... ";

    Allocator<int> alloc(0);
    std::array<Node, 128> nodes;

    for (int i = 0; i < 128; ++i) {
        nodes[i] = (alloc.allocate(i * 10));
    }

    for (int i = 0; i < 128; ++i) {

        int val = alloc.deallocate(nodes[i]);

        assert(val == i * 10);
    }

    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 3: Заполнение всех таблиц потока (32 * 128 = 4096 элементов)
static void alloc_test_fill_all() {
    std::cout << "[3] Fill all thread tables (4096 items)... ";

    Allocator<int> alloc(0);
    std::vector<Node> nodes;

    for (int i = 0; i < 4096; ++i) {
        nodes.push_back(alloc.allocate(i));
    }

    // Все таблицы заполнены — следующий allocate должен бросить исключение
    try {
        alloc.allocate(9999);
        assert(false);
    }
    catch (const std::runtime_error&) {
        // ok
    }

    for (int i = 0; i < 4096; ++i) {
        int val = alloc.deallocate(nodes[i]);
        assert(val == i);
    }

    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 4: Два потока, разные аллокаторы
static void alloc_test_two_threads() {
    std::cout << "[4] Two threads, separate allocators... ";

    std::atomic<int> sum{ 0 };
    std::array<int, 2000> arr1{};
    std::array<int, 2000> arr2{};
    std::thread t1([&] {
        Allocator<int> alloc(0);
        for (int i = 0; i < 2000; ++i) {
            Node node = alloc.allocate(i);
            arr1[i] = alloc.deallocate(node);
            sum += arr1[i];
        }
        });

    std::thread t2([&] {
        Allocator<int> alloc(1);
        for (int i = 2000; i < 4000; ++i) {
            Node node = alloc.allocate(i);
            arr2[i - 2000] = alloc.deallocate(node);
            sum += arr2[i - 2000];
        }
        });

    t1.join();
    t2.join();

    int expected = (3999 * 4000) / 2;
    expected = 0;
    for (int i = 0; i < 4000; ++i)
        expected += i;

    assert(sum.load() == expected);
    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 5: Четыре потока, общий счётчик
static void alloc_test_four_threads() {
    std::cout << "[5] Four threads, shared counter... ";

    constexpr int THREADS = 4;
    constexpr int OPS = 1000;
    constexpr int TOTAL = THREADS * OPS;

    std::atomic<int> counter{ 0 };
    std::atomic<long long> sum{ 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t] {
            Allocator<int> alloc(t % 16);
            for (int i = 0; i < OPS; ++i) {
                int val = counter.fetch_add(1);
                Node node = alloc.allocate(val);
                sum += alloc.deallocate(node);
            }
            });
    }

    for (auto& th : threads) th.join();

    long long expected = (long long)(TOTAL - 1) * TOTAL / 2;
    assert(sum.load() == expected);
    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 6: 128 потоков — проверка на прочность
static void alloc_test_heavy() {
    std::cout << "[6] 128 threads, light load... ";

    constexpr int THREADS = 128;
    constexpr int OPS = 200;
    constexpr int TOTAL = THREADS * OPS;

    std::atomic<int> counter{ 0 };
    std::atomic<long long> sum{ 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t] {
            Allocator<int> alloc(t % 16);  // 16 уникальных ID
            for (int i = 0; i < OPS; ++i) {
                int val = counter.fetch_add(1);
                Node node = alloc.allocate(val);
                sum += alloc.deallocate(node);
            }
            });
    }

    for (auto& th : threads) th.join();

    long long expected = (long long)(TOTAL - 1) * TOTAL / 2;
    assert(sum.load() == expected);
    std::cout << "OK\n";
}

// ============================================================
static int allocator_test() {
    alloc_test_basic();
    alloc_test_fill_table();
    alloc_test_fill_all();
    alloc_test_two_threads();
    alloc_test_four_threads();
    alloc_test_heavy();

    std::cout << "\nALL ALLOCATOR TESTS PASSED\n";
    return 0;
}