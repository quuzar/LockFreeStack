#pragma once
#include <atomic>
#include <thread>
#include <array>

#include "Structs.h"

template<typename T>
class Allocator {
public:
	Allocator() = default;
	Allocator(uint8_t id_thread) {
		for (int i = 0; i < 16; i++) {
			tables[i] = &pool_tables[i + id_thread*16];
			thread_id = id_thread;
		}
	}

	Node allocate(T value) {
		if (offset < max_offset) {
			uint16_t id_offset = offset++;
			tables[number_table]->add_value(value, id_offset);
			uint16_t _num_tab = number_table + (thread_id * 16);
			return { counter.fetch_add(1, std::memory_order_acq_rel), {_num_tab, id_offset}};
		}

		uint16_t free_offset = tables[number_table]->search();
		if (free_offset < max_offset) {
			tables[number_table]->add_value(value, free_offset);
			uint16_t _num_tab = number_table + (thread_id * 16);
			return { counter.fetch_add(1, std::memory_order_acq_rel), {_num_tab, free_offset} };
		}

		++number_table;

		if (number_table < max_tables) {
			offset = 1;
			tables[number_table]->add_value(value, 0);
			uint16_t _num_tab = number_table + (thread_id * 16);
			return { counter.fetch_add(1, std::memory_order_acq_rel), {_num_tab, 0} };
		}

		return { counter.fetch_add(1, std::memory_order_acq_rel), _search_offset() };
	}
	
	T deallocate(Node node) {
		uint16_t _tbl = node.position.table;
		uint16_t _off = node.position.offset;

		T _value = pool_tables[_tbl].ret_value(_off);

		return _value;
	}
	
	Position _search_offset() {
		for (uint16_t id_table = 0; Table<T>* _table : tables) {
			uint16_t _offset = _table->search();
			if (_offset < max_offset) {
				return { static_cast<uint16_t>(id_table + thread_id * max_tables), _offset };
			}
			++id_table;
		}

		throw "table overflow";
	}

	~Allocator() = default;

private:
	static inline std::atomic<uint32_t> counter{0};							// Глобальный счетчик

	static inline constexpr size_t value_size = sizeof(T);					// Размер типа
	static inline constexpr uint8_t max_offset = 64;						// MAX элементов в таблице
	static inline constexpr uint8_t max_tables = 16;
	static inline constexpr size_t max_pool_t = 4096;

	static inline std::array<Table<T>, max_pool_t> pool_tables;				// Глобальный пул таблиц
		
	alignas(64) uint8_t thread_id { 0 };
	alignas(16) uint16_t number_table{ 0 };									// Локальный счетчик текущей таблицы
	uint16_t offset{ 0 };													// Локальный счетчик текущего смещения
	
	std::array<Table<T>*, 16> tables;										// Локальный список таблиц
};