#pragma once
#include "Allocator.h"
#include <array>
#include <list>

template<typename T>
class LockStack {
public:
	LockStack() {
		tl.id = counter.fetch_add(1);
		if (tl.id >= MAX_ID) throw std::runtime_error("too many threads");
	}

	void push(T value) {
		Node* node = alloc_.allocate(value);
		StackNode* s_node = new StackNode();
		s_node->node.store(node);
		while (true) {
			StackNode* h = head.load();
			hazard_pointer[tl.id].store(h);
			s_node->prev.store(h);
			if (head.compare_exchange_weak(h, s_node)) {
				hazard_pointer[tl.id].store(nullptr);
				return;
			}
		}
	}

	T pop() {
		while (true) {
			StackNode* s_head = head.load();
			//if (!s_head) throw "empty";
			hazard_pointer[tl.id].store(s_head);
			StackNode* s_node = s_head->prev.load();

			if (head.compare_exchange_weak(s_head, s_node)) {
				Node* node = s_head->node.load();
				hazard_pointer[tl.id].store(nullptr);
				tl.retire_list.push_back(s_head);
				return alloc_.deallocate(node);
			}
 		}
	}

	void _clear_retire_list_() {
		std::array<StackNode*, MAX_ID> snapshot{};
		for (int id = 0; auto i : hazard_pointer) {
			snapshot[id] = i.load();
			++id;
		}

		tl.retire_list.remove_if([&](StackNode* node) {
			for (StackNode* hp : snapshot) {
				if (node == hp) return false; 
			}
			delete node;      
			return true;    
		});
	}

private:
	struct ThreadData {
		std::list<StackNode*> retire_list{};
		uint16_t id;                   
	};


	static constexpr uint16_t MAX_ID = 16;
	static inline thread_local ThreadData tl{};

	static inline std::array<std::atomic<StackNode*>, MAX_ID> hazard_pointer{};

	std::atomic<StackNode*> head{};
	static inline Allocator<T> alloc_{};
	static inline std::atomic<uint16_t> counter{ 0 };
};