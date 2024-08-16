#ifndef INCLUDE_GUARD_THREADPOOL_HPP
#define INCLUDE_GUARD_THREADPOOL_HPP

#include <cstdint>
#include <cstddef> 
#include <thread>
#include <exception> 
#include <utility>
#include <mutex>
#include <memory>
#include <any>
#include <deque>
#include <future>
#include <functional>
#include <type_traits>
#include <condition_variable>

namespace vsock {

    class Task {
    private:

        using task_type_t = enum class TaskType : std::uint8_t { DEFAULT, LOOP };
        using task_func_t = std::function<void(Task&)>;
        using cond_func_t = std::function<bool(Task&)>;

    public:

        Task(const Task&) = delete;
        Task& operator=(Task&&) = delete;
        Task& operator=(const Task&) = delete;

    public:

        Task() = default;

        Task(Task&& other) :
            vars_(std::move(other.vars_)),
            func_(std::exchange(other.func_, {})),
            cond_(std::exchange(other.cond_, {})),
            type_{ std::exchange(other.type_, task_type_t::DEFAULT) }
        {}

        Task(task_func_t&& func) :
            func_{ std::move(func) },
            type_{ TaskType::DEFAULT }
        {}

        template<typename... Args>
        void AddVariables(Args&&... args) {
            (vars_.push_back(std::move(args)), ...);
        }

        void SetTaskFuncion(task_func_t&& func) {
            func_ = std::move(func);
        }

        void SetConditionFunction(cond_func_t&& cond) {
            type_ = task_type_t::LOOP;
            cond_ = std::move(cond);
        }

        std::any GetVariable(std::size_t index) {
            return vars_[index];
        }

        std::any& GetVariableRef(std::size_t index) {
            return vars_[index];
        }

        bool operator()() {
            if (type_ == TaskType::LOOP) {
                if (cond_(*this)) {
                    func_(*this);
                    return true;
                }
                return false;
            }
            func_(*this);
            return false;
        }

    private:

        std::vector<std::any> vars_;
        task_func_t func_;
        cond_func_t cond_;
        task_type_t type_ = task_type_t::DEFAULT;

    };


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


        void AddAsyncTask(Task&& task);

        template <typename FuncType>
        void AddAsyncTask(FuncType&& task);        

        template <typename FuncType, typename RetType = std::invoke_result_t<std::decay_t<FuncType>>>
        [[nodiscard]] std::future<RetType> AddSyncTask(FuncType&& task);
        
        /*
        template <typename FuncType>
        void AddAsyncTask(FuncType&& task);
        */

        /*
        template <typename FuncType, typename RetType = std::invoke_result_t<std::decay_t<FuncType>>>
        [[nodiscard]] std::future<RetType> AddSyncTask(FuncType&& task);
        */

        /*
        template <class FuncType, class... Args>
        auto AddTask(FuncType&& task_func, Args&&... args) -> std::future<decltype(task_func(args...))>;
        */

    private:

        DestroyType destroy_type_;

        std::unique_ptr<std::thread[]> threads_;
        std::deque<Task> tasks_;

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

    inline void ThreadPool::AddAsyncTask(
        Task&& task
    ) {
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.emplace_back(
                std::move(task)
            );
        }
        tasks_available_cv_.notify_one();
    }

    template<typename FuncType>
    inline void ThreadPool::AddAsyncTask(FuncType&& task) {
        AddAsyncTask(
            Task(std::move(std::forward<FuncType>(task)))
        );
    }    

    template<typename FuncType, typename RetType>
    inline std::future<RetType> ThreadPool::AddSyncTask(FuncType&& task) {
        const std::shared_ptr<std::promise<RetType>> task_promise = std::make_shared<std::promise<RetType>>();
        AddAsyncTask(
            [task = std::forward<FuncType>(task), task_promise](Task& t) {
            try {
                if constexpr (std::is_void_v<RetType>) {
                    task();
                    task_promise->set_value();
                }
                else {
                    task_promise->set_value(task());
                }
            }
            catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                }
                catch (...) {
                }
            }
        });
        return task_promise->get_future();
    }

    /*
    template<typename FuncType>
    inline void ThreadPool::AddAsyncTask(FuncType&& task) {
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.emplace_back(std::forward<FuncType>(task));
        }
        tasks_available_cv_.notify_one();
    }
    */
    /*
    template<typename FuncType, typename RetType>
    inline std::future<RetType> ThreadPool::AddSyncTask(FuncType&& task) {
        const std::shared_ptr<std::promise<RetType>> task_promise = std::make_shared<std::promise<RetType>>();
        AddAsyncTask(
            [task = std::forward<FuncType>(task), task_promise] {
            try {
                if constexpr (std::is_void_v<RetType>) {
                    task();
                    task_promise->set_value();
                }
                else {
                    task_promise->set_value(task());
                }
            }
            catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                }
                catch (...) {
                }
            }
        });
        return task_promise->get_future();
    }
    */
    /*
    template<class FuncType, class ...Args>
    inline auto ThreadPool::AddTask(FuncType&& task_func, Args && ...args) -> std::future<decltype(task_func(args ...))> {

        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            if (!working_)
                throw std::runtime_error(
                    "Delegating task to a threadpool "
                    "that has been terminated or canceled.");
        }

        using return_t = decltype(task_func(args...));
        using future_t = std::future<return_t>;
        using task_t = std::packaged_task<return_t()>;

        auto bind_func = std::bind(std::forward<FuncType>(task_func), std::forward<Args>(args)...);
        std::shared_ptr<task_t> task = std::make_shared<task_t>(std::move(bind_func));
        future_t res = task->get_future();
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.emplace_back([task]() -> void {
                (*task)();
            });
        }

        tasks_available_cv_.notify_one();
        return res;
    }
    */



}

#endif // INCLUDE_GUARD_THREADPOOL_HPP
