#pragma once
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>
#include "LockFreeStack.h"

static constexpr int GLOBAL_POOL_SIZE = 32768;

static void print_header() {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "LOCK-FREE STACK BENCHMARK\n";
    std::cout << "Global node pool: " << GLOBAL_POOL_SIZE << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

static void test_independent(int stacks, int ops_per_stack) {
    LockFreeStack<int>::_initial_stack();

    int total_ops = stacks * ops_per_stack;
    if (total_ops > GLOBAL_POOL_SIZE) {
        ops_per_stack = GLOBAL_POOL_SIZE / stacks;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    std::atomic<long long> counter{ 0 };

    for (int i = 0; i < stacks; i++) {
        workers.emplace_back([ops_per_stack, &counter]() {
            LockFreeStack<int> stack;

            for (int j = 0; j < ops_per_stack; j++) {
                stack.push(j);
                counter++;
            }

            for (int j = 0; j < ops_per_stack; j++) {
                stack.pop();
                counter++;
            }
            });
    }

    for (auto& t : workers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Independent    | stacks: " << std::setw(3) << stacks
        << " | ops: " << std::setw(8) << counter.load()
        << " | time: " << std::setw(8) << std::fixed << std::setprecision(2) << ms << " ms"
        << " | ops/sec: " << std::setw(10) << std::fixed << std::setprecision(0)
        << (counter.load() / (ms / 1000.0)) << "\n";
}

static void test_many_small(int stacks, int ops) {
    LockFreeStack<int>::_initial_stack();

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;

    for (int i = 0; i < stacks; i++) {
        workers.emplace_back([ops]() {
            LockFreeStack<int> stack;

            for (int j = 0; j < ops; j++) stack.push(j);
            for (int j = 0; j < ops; j++) stack.pop();
            });
    }

    for (auto& t : workers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<double, std::milli>(end - start).count();
    long long total = stacks * ops * 2;

    std::cout << "Many small     | stacks: " << std::setw(3) << stacks
        << " | ops: " << std::setw(8) << total
        << " | time: " << std::setw(8) << std::fixed << std::setprecision(2) << ms << " ms"
        << " | ops/sec: " << std::setw(10) << std::fixed << std::setprecision(0)
        << (total / (ms / 1000.0)) << "\n";
}

static void test_sequential(int iterations) {
    LockFreeStack<int>::_initial_stack();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        LockFreeStack<int> stack;
        for (int j = 0; j < 1000; j++) stack.push(j);
        for (int j = 0; j < 1000; j++) stack.pop();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Sequential     | stacks: " << std::setw(3) << iterations
        << " | time: " << std::setw(8) << std::fixed << std::setprecision(2) << ms << " ms"
        << " | stacks/sec: " << std::setw(10) << std::fixed << std::setprecision(0)
        << (iterations / (ms / 1000.0)) << "\n";
}

static void test_capacity() {
    LockFreeStack<int>::_initial_stack();

    LockFreeStack<int> stack;
    int count = 0;

    try {
        while (true) {
            stack.push(count++);
        }
    }
    catch (const std::runtime_error& e) {
        std::cout << "Stack filled: " << count << " elements\n";
    }

    bool ok = true;
    for (int i = count - 2; i >= 0; i--) {
        if (stack.pop() != i) {
            ok = false;
            break;
        }
    }
    std::cout << "Pop " << (ok ? "OK" : "FAILED") << "\n";
}

static void test_pool_contention(int threads, int stacks_per_thread) {
    LockFreeStack<int>::_initial_stack();

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    std::atomic<int> total_stacks{ 0 };

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([stacks_per_thread, &total_stacks]() {
            for (int s = 0; s < stacks_per_thread; s++) {
                LockFreeStack<int> stack;
                for (int i = 0; i < 3000; i++) {
                    try { stack.push(i); }
                    catch (...) { break; }
                }
                total_stacks++;
            }
            });
    }

    for (auto& w : workers) w.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Pool contention| threads: " << threads
        << " | stacks: " << total_stacks.load()
        << " | time: " << std::setw(8) << std::fixed << std::setprecision(2) << ms << " ms\n";
}

static void run_benchmarks() {
    print_header();

    std::cout << "\n--- Independent stacks test ---\n";
    test_independent(1, 4000);
    test_independent(2, 2000);
    test_independent(4, 1000);
    test_independent(8, 500);
    test_independent(16, 250);
    test_independent(32, 125);
    test_independent(64, 64);
    test_independent(128, 32);

    std::cout << "\n--- Many small stacks test ---\n";
    test_many_small(10, 1000);
    test_many_small(20, 500);
    test_many_small(40, 250);

    std::cout << "\n--- Sequential stacks test ---\n";
    test_sequential(10);

    std::cout << "\n--- Capacity test ---\n";
    test_capacity();

    std::cout << "\n--- Pool contention test ---\n";
    test_pool_contention(4, 4);
    test_pool_contention(8, 4);
    test_pool_contention(16, 4);

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "BENCHMARK COMPLETED\n";
    std::cout << std::string(80, '=') << "\n";
}