#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include "Stack.h"

// ================== НАСТРОЙКИ ==================
constexpr int THREADS = 4;           // не 128, чтобы не убить пул
constexpr int OPS = 500;               // 500 < 1024 — безопасно для 16 таблиц

// ================== ТЕСТ 1: Базовый LIFO ==================
void test_lifo() {
    std::cout << "[1] LIFO order... ";
    LockFreeStack<int>::_initial_snode();
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

// ================== ТЕСТ 2: Один поток, много операций ==================
void test_single() {
    std::cout << "[2] Single thread, " << OPS << " ops... ";
    LockFreeStack<int>::_initial_snode();
    LockFreeStack<int> stack;

    for (int i = 0; i < OPS; ++i) stack.push(i);
    for (int i = OPS - 1; i >= 0; --i) assert(stack.pop() == i);
    std::cout << "OK\n";
}

// ================== ТЕСТ 3: Producer + Consumer ==================
void test_prod_cons() {
    std::cout << "[3] Producer/consumer, " << OPS << " items... ";
    LockFreeStack<int>::_initial_snode();
    LockFreeStack<int> stack;

    std::atomic<int> sum{ 0 };
    std::atomic<bool> done{ false };

    std::thread prod([&] {
        for (int i = 0; i < OPS; ++i) stack.push(i);
        done = true;
        });

    std::thread cons([&] {
        int received = 0;
        while (received < OPS) {
            try {
                sum += stack.pop();
                ++received;
            }
            catch (...) {
                std::this_thread::yield();
            }
        }
        });

    prod.join();
    cons.join();

    assert(sum.load() == OPS * (OPS - 1) / 2);
    std::cout << "OK\n";
}

// ================== ТЕСТ 4: Каждый поток со своим стеком ==================
void test_per_thread() {
    std::cout << "[4] " << THREADS << " threads, each own stack... ";
    LockFreeStack<int>::_initial_snode();

    std::atomic<int> counter{ 0 };
    std::atomic<long long> total{ 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&] {
            LockFreeStack<int> stack;
            for (int i = 0; i < OPS; ++i) {
                int val = counter.fetch_add(1);
                stack.push(val);
            }
            for (int i = 0; i < OPS; ++i) {
                total.fetch_add(stack.pop());
            }
            });
    }

    for (auto& th : threads) th.join();

    long long total_ops = THREADS * OPS;
    long long expected = total_ops * (total_ops - 1) / 2;

    std::cout << total.load() << std::endl;
    std::cout << expected << std::endl;
    assert(total.load() == expected);
    std::cout << "OK\n";
}

// ================== ТЕСТ 5: Общий стек, много потоков ==================
void test_shared() {
    std::cout << "[5] Shared stack, " << THREADS << " threads, " << OPS << " ops each... ";
    LockFreeStack<int>::_initial_snode();
    LockFreeStack<int> stack;

    std::atomic<int> sum{ 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < OPS; ++i) {
                if (i % 2 == 0) {
                    stack.push(i);
                }
                else {
                    try {
                        sum += stack.pop();
                    }
                    catch (...) {}
                }
            }
            });
    }

    for (auto& th : threads) th.join();
    std::cout << "OK (no crash)\n";
}

// ================== MAIN ==================
int main() {
    test_lifo();
    test_single();
    test_prod_cons();
    test_per_thread();
    //test_shared();

    std::cout << "\n✅ ALL TESTS PASSED\n";
    return 0;
}