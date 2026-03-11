#pragma once
#include "Allocator.h"
#include <vector>
#include <array>
#include <list>

template<typename T>
class LockFreeStack {
public:
	static void _initial_snode() {
		for (StackNode& _sn : _snode) {
			free_snode.push_back(&_sn);
		}

	}

	LockFreeStack() {
		spinlock_id.lock_write();
		if (!free_thread_id.empty()) {
			thread_id = free_thread_id.back();
			free_thread_id.pop_back();
			spinlock_id.unlock_write();
		}
		else {
			spinlock_id.unlock_write();
			thread_id = counter_thread.fetch_add(1, std::memory_order_acq_rel);
		}
		alloc_ = Allocator<T>(thread_id);
	}

	void push(T value) {
		spinlock_sn.lock_write();
		if (free_snode.empty()) throw;
		StackNode* _snode = free_snode.back();
		free_snode.pop_back();
		spinlock_sn.unlock_write();

		_snode->node = alloc_.allocate(value);
		StackNode* _shead{};

		while (true) {
			_shead = head_stack.load(std::memory_order_acquire);
			_snode->prev.store(_shead, std::memory_order_relaxed);
			if (head_stack.compare_exchange_weak(_shead, _snode, std::memory_order_acq_rel)) {
				return;
			}
		}
	}

	T pop() {
		while (true) {
			StackNode* _shead = head_stack.load(std::memory_order_acquire);
			if (!_shead) throw std::runtime_error("empty stack");

			hazard_pointer[thread_id].store(_shead, std::memory_order_release);

			StackNode* _snode = _shead->prev.load(std::memory_order_acquire);

			if (head_stack.compare_exchange_weak(_shead, _snode, std::memory_order_acq_rel)) {
				hazard_pointer[thread_id].store(nullptr, std::memory_order_release);

				T _value = alloc_.deallocate(_shead->node);

				return_list.push_back(_shead);

				scan_and_free();

				return _value;
			}
		}
	}

	void scan_and_free() {
		if (return_list.size() <= 16) return;

		std::vector<StackNode*> return_node;
		std::array<StackNode*, 256> snapshot{};

		int i = 0;
		for (std::atomic<StackNode*>& hp : hazard_pointer) {
			snapshot[i++] = hp.load(std::memory_order_acquire);
		}

		for (StackNode* node : return_list) {
			bool found = false;
			for (StackNode* hp : snapshot) {
				if (node == hp) {
					return_node.push_back(node);
					found = true;
					break;
				}
			}
			if (!found) {
				spinlock_sn.lock_write();
				free_snode.push_back(node);
				spinlock_sn.unlock_write();
			}
		}

		return_list = std::move(return_node);
	}

	~LockFreeStack() {
		spinlock_id.lock_write();
		free_thread_id.push_back(thread_id);
		spinlock_id.unlock_write();

	}

private:
	static inline std::atomic<uint8_t> counter_thread{0};
	static inline std::vector<uint8_t> free_thread_id{};
	static inline RW_spinlock spinlock_id{};
	static inline RW_spinlock spinlock_sn{};
	static inline std::atomic<StackNode*> head_stack{};
	static inline std::array< std::atomic<StackNode*>, 256> hazard_pointer{};

	uint8_t thread_id{};
	Allocator<T> alloc_;
	std::vector<StackNode*> return_list{};

	static inline std::array<StackNode, 32768> _snode{};
	static inline std::vector<StackNode*> free_snode = std::vector<StackNode*>{};
};