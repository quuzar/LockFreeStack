#pragma once
#include "LockFreeStack.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

static void test() {
	std::atomic<uint64_t> count{ 0 };
	std::vector<std::thread> list_tread{};
	int number{ 0 };
	LockFreeStack<int>::_initial_stack();
	while (number < 10) {
		for (int i = 0; i < 256; i++) {
			std::thread tr([&count]() {
				LockFreeStack<int> stack;
				for (int j = 1; j <= 524'288;) {
					stack.push(j++);
					stack.push(j++);
					stack.push(j++);
					stack.push(j++);
					stack.push(j++);
					stack.push(j++);
					stack.push(j++);
					stack.push(j++);

					uint64_t sum{ 0 };
					sum += stack.pop();
					sum += stack.pop();
					sum += stack.pop();
					sum += stack.pop();
					sum += stack.pop();
					sum += stack.pop();
					sum += stack.pop();
					sum += stack.pop();

					count.fetch_add(sum, std::memory_order_acq_rel);
				}
				});
			list_tread.push_back(std::move(tr));
		}

		for (auto& tr : list_tread)
			tr.join();

		list_tread.clear();

		std::cout << count.load() << "\n";
		count.store(0);
		number++;
	}
}