#pragma once
#include <atomic>
#include <queue>
#include <bit>

struct Node {
	uint32_t counter; // Счетчик
	uint16_t table;   // Номер таблицы
	uint16_t offset;  // Смещение
};

struct StackNode {
	std::atomic<Node*> node;      // Узел
 	std::atomic<StackNode*> prev; // Предыдущий Stack-узел
};

struct RW_spinlock {
	std::atomic<bool> flag{ false };
	std::atomic<uint16_t> count{ 0 };

	inline void lock_read() {
		count.fetch_add(1, std::memory_order_release);
		bool _f{ false };
		while (true) {
			if (flag.compare_exchange_weak(_f, false, std::memory_order_acq_rel)) {
				return;
			}
			_f = false;
			std::this_thread::yield();
		}
	}

	inline void unlock_read() {
		count.fetch_sub(1, std::memory_order_release);
	}

	inline void lock_write() {
		bool _f{ false };
		uint16_t _c{ 0 };
		while (true) {
			if (!flag.compare_exchange_weak(_f, true, std::memory_order_acq_rel)) {
				_f = false;
				std::this_thread::yield();
				continue;
			}
			if (count.compare_exchange_weak(_c, 0, std::memory_order_acq_rel)) {
				return;
			}
			_c = 0;
			std::this_thread::yield();
		}
	}

	inline void unlock_write() {
		flag.store(false, std::memory_order_release);
	}
};

template<typename T>
struct alignas(64 * sizeof(T)) Table {
	T nodes[64];
	std::atomic<uint64_t> free_mask{ ~0ULL };  // 1 = свободно

	Table() = default;

	inline uint16_t search() {
		uint16_t mask = free_mask.load(std::memory_order_acquire);
		return std::countr_zero(mask); 
	}

	inline void add_value(T value, uint16_t offset) {
		free_mask.fetch_or((~(1ULL << offset)), std::memory_order_release);
		nodes[offset] = value;
	}

	inline T get_value(uint16_t offset) {
		return nodes[offset];
	}

	inline T ret_value(uint16_t offset) {
		free_mask.fetch_or( (0ULL | 1ULL << offset), std::memory_order_release);
		return get_value(offset);
	}

	~Table() = default;
};