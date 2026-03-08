#pragma once
#include <atomic>
#include <vector>
#include <mutex>
#include <thread>
#include <shared_mutex>
#include "Structs.h"

template<typename T>
class Allocator {
public:
	void _create_table() {
		flag_create_table.store(true);
		Table<T>* _t{};
		try {
			_t = new Table<T>();
			{
				std::shared_lock lock(vector_mutex);
				tables.push_back(_t);
			}
		}
		catch(auto e) {
			if (!_t) delete _t;
			flag_create_table.store(false);
			throw std::runtime_error("error create table");
		}
		flag_create_table.store(false);
	}

	Node* allocate(T value) {
		while (true) {
			uint16_t _table = number_table.load();
			uint16_t _offset = offset.fetch_add(1);

			if (_offset >= max_offset) {
				offset.fetch_sub(1);
				{
					std::lock_guard<std::mutex> lock(table_mutex);
					while (flag_create_table.load()) {
						std::this_thread::yield();
					}

					if (_table != number_table.load()) {
						continue;
					}

					std::thread thr {&Allocator::_create_table, this }; // Используем в это время запасную таблицу
					thr.detach();

					number_table.fetch_add(1);
					offset.store(1);
				}
				++_table;
				_offset = 0;
			}

			{
				std::shared_lock lock(vector_mutex);
				tables[_table]->add_value(value, _offset);
			}

			Node* node = new Node();

			node->table = _table;
			node->offset = _offset;
			node->counter = counter.fetch_add(1);

			return node;
		}
	}
	
	T deallocate(Node* node) {
		std::shared_lock lock(vector_mutex);
		T value = tables[node->table]->get_value(node->offset);
		delete node;
		return value;
	}
	
	~Allocator() {
		for (auto table : tables)
			delete table;
	}

private:
	static inline std::atomic<uint16_t> number_table{0};
	static inline std::atomic<uint16_t> offset{0};
	static inline std::atomic<uint32_t> counter{0};
	static inline std::atomic<bool> flag_create_table{ false };
	static inline std::mutex table_mutex{};
	static inline std::shared_mutex vector_mutex{};
	Table<T>* current_table;
	std::vector<Table<T>*> tables;
	static constexpr size_t value_size = sizeof(T);
	static constexpr size_t max_offset = 4096 / value_size;
};