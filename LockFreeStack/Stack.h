#pragma once
#include "Allocator.h"
#include <vector>
#include <array>
#include <list>
#include <sstream>


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
        if (free_snode.empty()) {
            spinlock_sn.unlock_write();
            throw std::runtime_error("free_snode empty");
        }
        StackNode* _snode = free_snode.back();
        free_snode.pop_back();
        spinlock_sn.unlock_write();

        _snode->node.store(alloc_.allocate(value), std::memory_order_relaxed);

        StackNode* _shead{};
        while (true) {
            _shead = head_stack.load(std::memory_order_acquire);
            _snode->prev.store(_shead, std::memory_order_relaxed);
            if (head_stack.compare_exchange_weak(_shead, _snode,
                std::memory_order_acq_rel)) {
                return;
            }
        }
    }

    T pop() {
        while (true) {
            StackNode* _shead = head_stack.load(std::memory_order_acquire);
            if (!_shead) throw std::runtime_error("empty stack");

            hazard_pointer[thread_id].store(_shead, std::memory_order_release);

            StackNode* _snext = _shead->prev.load(std::memory_order_acquire);

            if (head_stack.compare_exchange_weak(_shead, _snext,
                std::memory_order_acq_rel)) {
                hazard_pointer[thread_id].store(nullptr, std::memory_order_release);

                T _value = alloc_.deallocate(_shead->node.load(std::memory_order_acquire));

                return_list.push_back(_shead);

                if (return_list.size() >= RETURN_LIST_LIMIT) {
                    scan_and_free();
                }

                return _value;
            }

            hazard_pointer[thread_id].store(nullptr, std::memory_order_release);
        }
    }

    void scan_and_free() {
        if (return_list.size() < RETURN_LIST_LIMIT / 2) return;

        std::vector<StackNode*> keep;
        keep.reserve(return_list.size());

        std::array<StackNode*, 256> snapshot;
        for (size_t i = 0; i < hazard_pointer.size(); ++i) {
            snapshot[i] = hazard_pointer[i].load(std::memory_order_acquire);
        }

        for (StackNode* node : return_list) {
            bool in_use = false;
            for (StackNode* hp : snapshot) {
                if (node == hp) {
                    in_use = true;
                    break;
                }
            }

            if (in_use) {
                keep.push_back(node);
            }
            else {
                spinlock_sn.lock_write();
                free_snode.push_back(node);
                spinlock_sn.unlock_write();
            }
        }

        return_list = std::move(keep);
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
	static inline RW_spinlock spinlock{};
	static inline std::atomic<StackNode*> head_stack{};
	static inline std::array< std::atomic<StackNode*>, 256> hazard_pointer{};
    static inline constexpr uint8_t RETURN_LIST_LIMIT{ 16 };

	uint8_t thread_id{};
	Allocator<T> alloc_;
	std::vector<StackNode*> return_list{};

    
	static inline std::array<StackNode, 32768> _snode{};
	static inline std::vector<StackNode*> free_snode = std::vector<StackNode*>{};
};