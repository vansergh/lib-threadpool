#ifndef INCLUDE_GUARD_THREADPOOL_HPP
#define INCLUDE_GUARD_THREADPOOL_HPP

#include <cstdint>
#include <cstddef> 
#include <thread>
#include <mutex>
#include <memory>
#include <deque>
#include <condition_variable>

#include <task.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // ThreadPool class declaration
    ////////////////////////////////////////////////////////////////////////////////

    class ThreadPool {
    public:

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

    public:

        enum class DestroyType : std::uint8_t {
            SMOOTH,
            SHARP
        };

        ThreadPool();
        ThreadPool(const DestroyType destroy_type);
        ThreadPool(const std::uint32_t concurency);
        ThreadPool(const std::uint32_t concurency, const DestroyType destroy_type);
        ~ThreadPool();

        void Wait();
        void Pause();
        void Continue();
        void ClearTasks();

        void Reset();
        void Reset(const DestroyType destroy_type);
        void Reset(const std::uint32_t concurency);
        void Reset(const std::uint32_t concurency, const DestroyType destroy_type);

        void AddSyncTask(std::unique_ptr<Task> task);
        void AddAsyncTask(std::unique_ptr<Task> task);

        template<typename F, typename...Args>
        auto AddSyncTask(F&& job, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

        template<typename F, typename...Args>
        void AddAsyncTask(F&& job, Args&&... args);

    private:

        DestroyType destroy_type_;

        std::unique_ptr<std::thread[]> threads_;
        std::deque<std::unique_ptr<Task>> tasks_;

        std::uint32_t threads_count_;
        std::uint64_t tasks_running_;

        bool working_;
        bool paused_;
        bool waiting_;

        mutable std::mutex tasks_mutex_;

        std::condition_variable tasks_available_cv_;
        std::condition_variable tasks_done_cv_;

        [[nodiscard]] std::uint32_t ChooseThreadsCount_(const std::uint32_t threads_count) const noexcept;
        void CreateThreads_();
        void StopThreads_();
        void DestroyThreads_();
        void Finish_();
        void Process_(const std::uint32_t thread_index);

    };

    //////////////////////////////////////////////////////////////////////////////////
    // ThreadPool class defenition (template methods)
    ////////////////////////////////////////////////////////////////////////////////

    template<typename F, typename...Args>
    auto ThreadPool::AddSyncTask(F&& job, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        std::unique_ptr<Task> task_ptr(std::make_unique<Task>());
        auto result = task_ptr->SetSyncJob(std::move(job), std::move(args)...);
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.push_back(std::move(task_ptr));
        }
        tasks_available_cv_.notify_one();
        return std::move(result);
    }

    template<typename F, typename...Args>
    void ThreadPool::AddAsyncTask(F&& job, Args&&... args) {
        std::unique_ptr<Task> task_ptr(std::make_unique<Task>());
        task_ptr->SetAsyncJob(std::move(job), std::move(args)...);
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.push_back(std::move(task_ptr));
        }
        tasks_available_cv_.notify_one();
    }

}

#endif // INCLUDE_GUARD_THREADPOOL_HPP
