#pragma once
#include "Allocator.h"
#include <vector>
#include <array>
#include <atomic>
#include <thread>

template<typename T>
class LockFreeStack {
public:
    static void _initial_stack() {
        free_lock.lock();
        for (auto& snode : snode_pool)
            free_nodes.push_back(&snode);
        free_lock.unlock();
    }

    LockFreeStack() {
        uint8_t id;
        id_lock.lock();
        if (free_ids.empty()) {
            id = global_id_counter.fetch_add(1, std::memory_order_acq_rel);
        }
        else {
            id = free_ids.back();
            free_ids.pop_back();
        }
        id_lock.unlock();
        alloc_ = Allocator<T>(id);
        thread_id = id;
    }

    ~LockFreeStack() {
        id_lock.lock();
        free_ids.push_back(thread_id);
        id_lock.unlock();

        do {
            std::this_thread::yield();
            scan_retired();
        } while (!retired_list.empty());
    }

    void push(T _value) {
        Node _node = alloc_.allocate(_value);

        free_lock.lock();
        if (free_nodes.empty()) {
            free_lock.unlock();
            alloc_.deallocate(_node);
            throw std::runtime_error("Stack overflow");
        }
        StackNode* _snode = free_nodes.back();
        free_nodes.pop_back();
        free_lock.unlock();

        _snode->node.store(_node, std::memory_order_relaxed);
        while (true) {
            StackNode* _head_snode = head.load(std::memory_order_acquire);
            hazard_ptr[thread_id].store(_head_snode, std::memory_order_release);

            _snode->prev.store(_head_snode, std::memory_order_relaxed);

            if (head.compare_exchange_weak(_head_snode, _snode, std::memory_order_acq_rel)) {
                hazard_ptr[thread_id].store(nullptr, std::memory_order_release);
                return;
            }
            hazard_ptr[thread_id].store(nullptr, std::memory_order_release);
        }
    }

    T pop() {
        while (true) {
            StackNode* _head_snode = head.load(std::memory_order_acquire);
            if (!_head_snode) throw std::runtime_error("Stack empty");

            hazard_ptr[thread_id].store(_head_snode, std::memory_order_release);

            StackNode* _snode = _head_snode->prev.load(std::memory_order_acquire);
            
            if (head.compare_exchange_weak(_head_snode, _snode, std::memory_order_acq_rel)) {
                Node _node = _head_snode->node.load(std::memory_order_acquire);
                T _value = alloc_.deallocate(_node);
                retired_list.push_back(_head_snode);
                if (retired_list.size() >= RETIRE_LIMIT) {
                    scan_retired();
                }
                hazard_ptr[thread_id].store(nullptr, std::memory_order_release);
                return _value;
            }

            hazard_ptr[thread_id].store(nullptr, std::memory_order_release);
        }
    }

    void scan_retired() {
        std::this_thread::yield();
        std::this_thread::yield();
        std::vector<StackNode*> hazard_vector{}, return_vector{}, free_vector{};
        bool flag = true;

        for (auto& _atomic_snode : hazard_ptr) {
            StackNode* _snode = _atomic_snode.load(std::memory_order_acquire);
            if (_snode) {
                hazard_vector.push_back(_snode);
            }
        }

        for (auto _snode : retired_list) {
            for (auto _hz_snode : hazard_vector) {
                if (_snode == _hz_snode) {
                    return_vector.push_back(_snode);
                    flag = false;
                    break;
                }
            }

            if (flag) {
                free_vector.push_back(_snode);
            }
            flag = true;
        }

        free_lock.lock();
        for (auto _snode : free_vector) {
            free_nodes.push_back(_snode);
        }
        free_lock.unlock();

        retired_list = std::move(return_vector);
    }

private:
    uint8_t thread_id{};
    Allocator<T> alloc_{};
    std::vector<StackNode*> retired_list{};
public:
    static inline std::atomic<uint8_t> global_id_counter{ 0 };
    static inline std::vector<uint8_t> free_ids{};
    static inline write_spinlock id_lock{};

    static inline std::atomic<StackNode*> head{ nullptr };
    static inline std::array<std::atomic<StackNode*>, Allocator<T>::MAX_THREADS> hazard_ptr{};
    static inline constexpr size_t RETIRE_LIMIT = 32;

    static inline std::array<StackNode, 32768> snode_pool{};
    static inline std::vector<StackNode*> free_nodes{};
    static inline write_spinlock free_lock{};
};