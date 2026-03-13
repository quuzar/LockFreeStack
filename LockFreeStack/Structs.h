#pragma once
#include <atomic>
#include <queue>
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

struct write_spinlock {
	inline void lock() {
		bool expected = false;
		while (!_flag.compare_exchange_weak(expected, true, std::memory_order_acq_rel)) {
			expected = false;
			std::this_thread::yield();
		}
	}

	inline void unlock() {
		_flag.store(false, std::memory_order_release);
	}
private:
	std::atomic<bool> _flag;
};

template<typename T>
struct alignas(128) Table {
	static constexpr uint16_t SIZE = 128;

	Table() = default;

	inline uint16_t search() {
		uint16_t _free_offset;
		_queue_spin.lock();
		if (free_nodes.empty()) {
			_free_offset = SIZE;
		}
		else {
			_free_offset = free_nodes.front();
			free_nodes.pop();
		}
		_queue_spin.unlock();
		return _free_offset;
	}

	inline void set_value(T value, uint16_t offset) {
		nodes[offset].store(value, std::memory_order_release);
	}

	inline T get_value(uint16_t offset) {
		T _value = nodes[offset].load(std::memory_order_acquire);
		_queue_spin.lock();
		free_nodes.push(offset);
		_queue_spin.unlock();
		return _value;
	}

	inline uint16_t next() {
		uint16_t _next_offset = next_free.fetch_add(1, std::memory_order_release);
		if (_next_offset < SIZE)
			return _next_offset;
		next_free.fetch_sub(1, std::memory_order_release);
		return search();
	}
private:
	write_spinlock _queue_spin{};

	std::atomic<uint16_t> next_free{ 0 };
	std::array<std::atomic<T>, SIZE> nodes;
	std::queue<uint16_t> free_nodes;
};

static std::ostream& operator<<(std::ostream& os, Node node) {
	os << node.position.table << ":" << node.position.offset;
	return os;
}