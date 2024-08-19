#ifndef INCLUDE_GUARD_QUEUE_HPP
#define INCLUDE_GUARD_QUEUE_HPP

#include <deque>
#include <mutex>
#include <memory>
#include <utility>

#include <task.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // TaskQueue class declaration
    ////////////////////////////////////////////////////////////////////////////////

    using value_t = std::unique_ptr<Task>;
    using deque_t = std::deque<value_t>;

    class TaskQueue : private deque_t {
        friend class ThreadPool;

        void PushBack(value_t&& task) {
            const std::scoped_lock rw_lock(mtx_);
            deque_t::push_back(std::move(task));
        }

        void Clear() noexcept {
            const std::scoped_lock rw_lock(mtx_);
            deque_t::clear();
        }

        bool Empty() const noexcept {
            const std::scoped_lock rw_lock(mtx_);
            return deque_t::empty();
        }

        void PopFront(value_t& task) noexcept {
            const std::scoped_lock rw_lock(mtx_);
            task = std::exchange(deque_t::front(), {});
            deque_t::pop_front();
        }

    private:
        mutable std::mutex mtx_;
    };

}

#endif