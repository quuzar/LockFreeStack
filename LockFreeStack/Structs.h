#pragma once
#include <atomic>
#include <bit>

struct Position {
	uint16_t table;    // Таблица
	uint16_t offset;   // Смещение
};

struct Node {
	uint32_t counter;  // Счетчик
	Position position; // Позиция
};

struct StackNode {
	StackNode() = default;

	StackNode(Node _node, StackNode* _snode)
		: node{ _node }, prev{ _snode } {};

	StackNode(StackNode& _snode)
		: node{ _snode.node.load(std::memory_order_acquire)}, 
		  prev{ _snode.prev.load(std::memory_order_acquire)} { };

	StackNode(StackNode&&) = delete;

	inline StackNode& operator=(StackNode& _snode) {
		node.store(_snode.node.load(std::memory_order_acquire), std::memory_order_relaxed);
		prev.store(_snode.prev.load(std::memory_order_acquire), std::memory_order_acq_rel);
	};

	StackNode& operator=(StackNode&&) = delete;

	std::atomic<Node> node;			// Узел
 	std::atomic<StackNode*> prev;	// Предыдущий Stack-узел

	~StackNode() = default;
};

struct RW_spinlock {
	std::atomic<uint16_t> count{ 0 };
	std::atomic<bool> flag{ false };

	inline void lock_read() {
		bool _f{ false };
		while (true) {
			count.fetch_add(1, std::memory_order_release);

			if (flag.compare_exchange_weak(_f, false, std::memory_order_acq_rel)) {
				return;
			}
			_f = false;
			count.fetch_sub(1, std::memory_order_acq_rel);
			std::this_thread::yield();
			std::this_thread::yield();
		}
	}

	inline void unlock_read() {
		count.fetch_sub(1, std::memory_order_release);
	}

	inline void lock_write() {
		bool _f{ false };
		while (true) {
			if (!flag.compare_exchange_weak(_f, true, std::memory_order_acq_rel)) {
				_f = false;
				std::this_thread::yield();
				continue;
			}
			if (count.load(std::memory_order_acquire) == 0) {
				return;  
			}
			flag.store(false, std::memory_order_release);
			std::this_thread::yield();
		}
	}

	inline void unlock_write() {
		flag.store(false, std::memory_order_release);
	}
};

template<typename T>
struct alignas(64 * sizeof(T)) Table {
	std::atomic<uint64_t> free_mask{ ~0ULL };  // 1 = свободно
	std::array<T, 64> nodes{};

	Table() = default;

	inline uint16_t search() {
		uint64_t mask = free_mask.load(std::memory_order_acquire);
		return std::countr_zero(mask); 
	}

	inline void add_value(T value, uint16_t offset) {
		rw_spin.lock_write();
		free_mask.fetch_and((~(1ULL << offset)), std::memory_order_release);
		nodes[offset] = value;
		rw_spin.unlock_write();
	}

	inline T _get_value(uint16_t offset) {
		return nodes[offset];
	}

	inline T ret_value(uint16_t offset) {
		free_mask.fetch_or( (0ULL | 1ULL << offset), std::memory_order_release);
		rw_spin.lock_read();
		T _value = _get_value(offset);
		rw_spin.unlock_read();
		return _value;
	}

	~Table() = default;

	RW_spinlock rw_spin{};
};