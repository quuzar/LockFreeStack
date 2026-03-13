#pragma once
#include <atomic>
#include <thread>
#include <array>

#include "Structs.h"

template<typename T>
class Allocator {
public:
    Allocator() = default;

    Allocator(uint8_t id_thread) : _current_thread_id(id_thread) {
        if (id_thread >= MAX_THREADS) throw std::runtime_error("invalid thread id");
        for (uint16_t i = 0; i < MAX_TABLES; ++i) {
            _tables[i] = &global_pool[id_thread * MAX_TABLES + i];
        }
    }

    inline Node allocate(T value) {
        while (_flag_tables.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        uint16_t table = _current_table.load(std::memory_order_acquire);
        uint16_t offset = _tables[table].load(std::memory_order_acquire)->next();

        if (offset < MAX_OFFSET) {
            _tables[table].load(std::memory_order_acquire)->set_value(value, offset);
            return make_node(table, offset);
        }

        bool _f{ false };

        if (_flag_tables.compare_exchange_strong(_f, true, std::memory_order_acq_rel)) {
            table = _current_table.fetch_add(1, std::memory_order_acq_rel);
            if (table < MAX_TABLES - 1) {
                _flag_tables.store(false, std::memory_order_relaxed);
            }
            else
                throw std::runtime_error("Table overflow");
        }
            
        return allocate(value);
    }

    T deallocate(Node node) {
        uint16_t table = node.position.table;
        return global_pool[table].get_value(node.position.offset);
    }

    static inline constexpr uint16_t MAX_TABLES = 32;
    static inline constexpr uint16_t MAX_THREADS = 256;
    static inline constexpr uint16_t MAX_OFFSET = 128;
    static inline constexpr uint16_t POOL_SIZE = MAX_THREADS * MAX_TABLES;

private:
    inline Node make_node(uint16_t local_table, uint16_t offset) {
        uint16_t table = local_table + (_current_thread_id * MAX_TABLES);
        return {counter.fetch_add(1, std::memory_order_acq_rel), {table, offset} };
    }

    static inline std::atomic<uint32_t> counter{ 0 };

    static inline std::array<Table<T>, POOL_SIZE> global_pool;

    uint8_t _current_thread_id{ 0 };
    std::atomic<uint16_t> _current_table{0};
    std::array<std::atomic<Table<T>*>, MAX_TABLES> _tables{};

    std::atomic<bool> _flag_tables{ false };
};