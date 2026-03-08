#pragma once
#include <atomic>
#include <queue>

struct Node {
	uint32_t counter; // Счетчик
	uint16_t table;   // Номер таблицы
	uint16_t offset;  // Смещение
};

struct StackNode {
	std::atomic<Node*> node;
	std::atomic<StackNode*> prev;
};

template<typename T>
struct raw_table {
	alignas(4096) T table[4096/sizeof(T)];
};

template<typename T>
struct Table {
	Table()  {
		_table = new raw_table<T>{};
		count.store(0);
	};

	void add_value(T value, uint16_t offser) {
		_table->table[offser] = value;
		count.fetch_add(1);
	}

	T get_value(uint16_t node) {
		count.fetch_sub(1);
		return _table->table[node];
	}

	raw_table<T>* _table;        // Ссылка на таблицу
	std::atomic<uint16_t> count; // Атомарный счетчик кол-во

	~Table() {
		delete _table;
	}
};