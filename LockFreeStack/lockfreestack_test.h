#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <array>
#include <cassert>
#include "LockFreeStack.h"

// ============================================================
// ТЕСТ 1: Базовый LIFO порядок
static void stack_test_basic() {
    std::cout << "[1] Basic LIFO... ";
    LockFreeStack<int>::_initial_stack();
    LockFreeStack<int> stack;

    stack.push(10);
    stack.push(20);
    stack.push(30);

    assert(stack.pop() == 30);
    assert(stack.pop() == 20);
    assert(stack.pop() == 10);

    try {
        stack.pop();
        assert(false);
    }
    catch (const std::runtime_error&) {}

    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 2: Один поток, 1000 элементов
static void stack_test_1000() {
    std::cout << "[2] One thread, 1000 items... ";
    LockFreeStack<int>::_initial_stack();
    LockFreeStack<int> stack;

    for (int i = 0; i < 1000; ++i) stack.push(i);
    for (int i = 999; i >= 0; --i) assert(stack.pop() == i);

    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 3: Два потока, один стек
static void stack_test_two_threads() {
    std::cout << "[3] Two threads, shared stack... ";
    LockFreeStack<int>::_initial_stack();
    LockFreeStack<int> stack;

    std::atomic<int> push_count{ 0 };
    std::atomic<int> pop_count{ 0 };
    std::atomic<long long> sum{ 0 };

    std::thread t1([&] {
        for (int i = 0; i < 500; ++i) {
            stack.push(i);
            push_count++;
        }
        });

    std::thread t2([&] {
        for (int i = 500; i < 1000; ++i) {
            stack.push(i);
            push_count++;
        }
        });

    t1.join();
    t2.join();

    while (pop_count.load() < push_count.load()) {
        try {
            sum += stack.pop();
            pop_count++;
        }
        catch (...) {}
    }

    assert(push_count.load() == pop_count.load());
    assert(sum.load() == (999 * 1000) / 2);
    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 4: Четыре потока, один стек
static void stack_test_four_threads() {
    std::cout << "[4] Four threads, shared stack... ";
    LockFreeStack<int>::_initial_stack();
    LockFreeStack<int> stack;

    std::atomic<int> push_count{ 0 };
    std::atomic<int> pop_count{ 0 };
    std::atomic<long long> sum{ 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            int start = t * 250;
            int end = start + 250;
            for (int i = start; i < end; ++i) {
                stack.push(i);
                push_count++;
            }
            });
    }

    for (auto& th : threads) th.join();

    while (pop_count.load() < push_count.load()) {
        try {
            sum += stack.pop();
            pop_count++;
        }
        catch (...) {}
    }

    assert(push_count.load() == pop_count.load());
    assert(sum.load() == (999 * 1000) / 2);
    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 5: Восемь потоков, один стек
static void stack_test_eight_threads() {
    std::cout << "[5] Eight threads, shared stack... ";
    LockFreeStack<int>::_initial_stack();
    LockFreeStack<int> stack;

    std::atomic<int> push_count{ 0 };
    std::atomic<int> pop_count{ 0 };
    std::atomic<long long> sum{ 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t] {
            int start = t * 125;
            int end = start + 125;
            for (int i = start; i < end; ++i) {
                stack.push(i);
                push_count++;
            }
            });
    }

    for (auto& th : threads) th.join();

    while (pop_count.load() < push_count.load()) {
        try {
            sum += stack.pop();
            pop_count++;
        }
        catch (...) {}
    }

    assert(push_count.load() == pop_count.load());
    assert(sum.load() == (999 * 1000) / 2);
    std::cout << "OK\n";
}

// ============================================================
// ТЕСТ 6: Проверка лимита памяти (4096 элементов)
static void stack_test_memory_limit() {
    std::cout << "[6] Memory limit (4096 items)... ";
    LockFreeStack<int>::_initial_stack();
    LockFreeStack<int> stack;

    for (int i = 0; i < 4096; ++i) stack.push(i);

    try {
        stack.push(4096);
        assert(false);
    }
    catch (const std::runtime_error&) {}

    for (int i = 4095; i >= 0; --i) assert(stack.pop() == i);
    std::cout << "OK\n";
}

// ============================================================
static int stack_test() {
    stack_test_basic();
    stack_test_1000();
    stack_test_two_threads();
    stack_test_four_threads();
    stack_test_eight_threads();
    stack_test_memory_limit();

    std::cout << "\nALL STACK TESTS PASSED\n";
    return 0;
}