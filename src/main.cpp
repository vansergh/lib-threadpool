#include <atomic>
#include <numeric>
#include <string>
#include <thread>
#include <cstddef>
#include <chrono>
#include <iostream>
#include <random>
#include <list>
#include <mutex>
#include <threadpool.hpp>
#include <queue.hpp>
#include <varlist.hpp>

using namespace std;
using namespace vsock;
using namespace chrono;

static std::random_device dev;
static std::mt19937 rng(dev());

static std::mutex mtx_;

int RandomN(const int from, const int to) {
    std::uniform_int_distribution<std::mt19937::result_type> dist(from, to);
    return dist(rng);
}

std::size_t HardTest2(std::size_t size) {
    std::size_t i, num = 1, primes = 0;
    while (num <= size) {
        i = 2;
        while (i <= num) {
            if (num % i == 0)
                break;
            i++;
        }
        if (i == num)
            primes++;
        num++;
    }
    return primes;
}

bool HardTest1(std::size_t size) {
    std::vector<int> arr(size);
    std::iota(arr.begin(), arr.end(), 1);
    vector<int> res;
    for (std::size_t i : arr) {
        res.insert(res.begin(), i);
    }
    arr.clear();
    for (std::size_t i : res) {
        arr.insert(arr.begin(), i);
    }
    return true;
}

void PrintTaskWithID(int id, const int sleep_from, const int sleep_to) {
    int time = RandomN(sleep_from, sleep_to);

    mtx_.lock();
    cout << "PrintTaskWithID: task# " << id << " will sleep " << time << endl;
    mtx_.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(time));

    mtx_.lock();
    cout << "PrintTaskWithID: task# " << id << " waked up after " << time << endl;
    mtx_.unlock();
}


void PrintTask(const int sleep_from, const int sleep_to) {
    int time = RandomN(sleep_from, sleep_to);

    mtx_.lock();
    cout << "PrintTask: " << " will sleep " << time << endl;
    mtx_.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(time));

    mtx_.lock();
    cout << "PrintTask: " << " waked up after " << time << endl;
    mtx_.unlock();
}

void RunTests() {
    cout << "\n=========================================================================\n"s;
    ThreadPool pool;

    {
        cout << "Test #L1: -------------------\n";
        pool.AddAsyncTask([](int a, int b) {
            cout << "a + b = " << (a + b) << "\n";
        }, 10, 20);
        pool.Wait();
    }

    {
        cout << "Test #L2: -------------------\n";

        cout << "Result will be in 10 msec\n";

        auto result = pool.AddSyncTask([](int a, int b) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return a * b;
        }, 6, 10);

        cout << "a * b = " << result.get() << "\n";
        pool.Wait();
    }

    {
        cout << "Test #L3: -------------------\n";

        int val = 10;

        auto result = pool.AddSyncTask([](int& a) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            a = a * 10;
        }, std::ref(val));

        cout << "val = " << val << '\n';
        cout << "result.wait();\n";
        result.wait();

        cout << "val = " << val << '\n';
        pool.Wait();
    }

    {
        cout << "Test #L4: -------------------\n";

        int val = 10;

        auto result = pool.AddSyncTask([](int& a, ThreadPool* pool) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            a = a * 10;
            auto res = pool->AddSyncTask([](int& b) {
                b = b * 5;
            }, std::ref(a));
            res.wait();
            auto res2 = pool->AddSyncTask([](int& b) -> int {
                return b * 10;
            }, std::ref(a));
            cout << "thread> a = " << a << '\n';
            cout << "thread> res2 = " << res2.get() << '\n';
        }, std::ref(val), &pool);

        cout << "val = " << val << '\n';
        cout << "result.wait();\n";
        result.wait();

        cout << "val = " << val << '\n';
        pool.Wait();
    }

    {
        cout << "Test #L5: -------------------\n";
        std::vector<int> vec;
        std::atomic_bool inited = false;
        std::atomic_bool processed = false;

        pool.AddAsyncTask([](std::atomic_bool& p) {
            while (!p);
            cout << "processed!\n";
        }, std::ref(processed));

        auto fut1 = pool.AddSyncTask([](std::atomic_bool& i) -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            i = true;
            return 100;
        }, std::ref(inited));

        auto fut2 = pool.AddSyncTask([](std::vector<int>& v, std::future<int>& r, std::atomic_bool& i) {
            int count = r.get();
            cout << "thread1> inited = " << i << '\n';
            cout << "thread1> count = " << count << '\n';
            v.resize(count);
            for (int z = 0; z < count; ++z) {
                v[z] = z;
            }
            cout << "thread1> v.size = " << v.size() << '\n';

        }, std::ref(vec), std::ref(fut1), std::ref(inited));

        pool.AddAsyncTask([](std::atomic_bool& i, std::future<void>& f, std::vector<int>& v, std::atomic_bool& p) {
            while (!i);
            cout << "thread2> inited!\n";
            f.wait();
            cout << "thread2> fut received!\n";
            int c = v.size() * 2;
            v.resize(c);
            for (int z = 0; z < c; ++z) {
                v[z] = 999;
            }
            cout << "thread2> v.size = " << v.size() << '\n';
            cout << "thread2> v[10] = " << v[10] << '\n';
            p = true;
        }, std::ref(inited), std::ref(fut2), std::ref(vec), std::ref(processed));

        pool.Wait();
    }

    {
        cout << "Test #L6: -------------------\n";
        std::unique_ptr<Task> task = std::make_unique<Task>();
        task->vars.Add(0);
        task->vars.Add(10);
        task->vars.Add("hello"s);
        task->SetCondition([](Task& task) -> bool {
            const int it = task.vars.Get<int>(0);
            const int to = task.vars.Get<int>(1);
            return it < to;
        }, std::ref(*task));
        task->SetLoopJob([](Task& task) -> void {
            int& it = task.vars.Get<int>(0);
            const std::string str = task.vars.Get<std::string>(2);
            cout << "loop #" << it << ": " << str << '\n';
            ++it;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }, std::ref(*task));
        pool.AddAsyncTask(std::move(task));
        pool.Wait();
    }

    {
        cout << "Test #L7: -------------------\n";
        std::unique_ptr<Task> task = std::make_unique<Task>();
        std::atomic_bool c1{ true };
        task->vars.Add(std::ref(c1));
        task->SetCondition([](Task& task) -> bool {
            std::atomic_bool& c = task.vars.Get<std::reference_wrapper<std::atomic_bool>>(0);
            return c;
        }, std::ref(*task));
        task->SetLoopJob([](Task& task, ThreadPool* pool) -> void {
            std::atomic_bool& c = task.vars.Get<std::reference_wrapper<std::atomic_bool>>(0);
            mtx_.lock();
            cout << "loop start. c = " << c << "\n";
            mtx_.unlock();
            auto ret = pool->AddSyncTask([]() -> pair<int, int> {
                return std::make_pair(::RandomN(1, 100), ::RandomN(1, 100));
            });
            auto res = ret.get();
            cout << "first = " << res.first << ", second = " << res.second << "\n";
            if (res.first > res.second) {
                c = false;
            }
            cout << "loop end. c = " << c << "\n";
        }, std::ref(*task), &pool);
        pool.AddAsyncTask(std::move(task));
        pool.Wait();
    }

    {
        cout << "Test #L8: -------------------\n";
        auto work = [](Task& task, const std::string& str, int& r, ThreadPool* p) {
            cout << "Hello, " << str << "!\n";
            task.vars.Add(str + "123"s);
            std::unique_ptr<Task> t(std::make_unique<Task>());
            r = 10;
            cout << "r = " << r << '\n';
            t->vars.Add(std::ref(r));
            auto subjob = [](Task& ts) {
                int& v = ts.vars.Get<std::reference_wrapper<int>>(0);
                v *= 2;
                cout << "v = " << v << '\n';
            };
            t->SetAsyncJob(subjob, std::ref(*t));
            p->AddAsyncTask(std::move(t));
        };
        int result{ 0 };
        std::unique_ptr<Task> task(std::make_unique<Task>());
        task->SetAsyncJob(work, std::ref(*task), "Bro"s, std::ref(result), &pool);

        pool.AddAsyncTask(std::move(task));
        pool.Wait();
        cout << "result = " << result << '\n';
    }

    {
        cout << "Test #G1: -------------------\n";
        for (int z = 0; z < 10; ++z) {
            pool.AddAsyncTask(PrintTask, 1, 1000);
        }
        pool.Wait();
    }

    {
        cout << "Test #G2: -------------------\n";
        for (int z = 0; z < 10; ++z) {
            pool.AddAsyncTask(PrintTaskWithID, z, 1, 1000);
        }
        pool.Wait();
    }

    {
        cout << "Test #G3: -------------------\n";
        std::vector<std::future<bool>> results;
        for (int z = 0; z < 20; ++z) {
            results.emplace_back(pool.AddSyncTask(HardTest1, 10000));
        }
        pool.AddAsyncTask([&](std::vector<std::future<bool>>& r) {
            auto zero = std::chrono::seconds(0);
            while (!r.empty()) {
                for (auto it = r.begin(); it != r.end();) {
                    if (it->wait_for(zero) == std::future_status::ready) {
                        it = r.erase(it);
                        mtx_.lock();
                        cout << "result is ready\n";
                        mtx_.unlock();
                    }
                    else {
                        ++it;
                    }
                }
            }
            cout << "all done!\n";
        }, std::ref(results));
        cout << "waiting results...\n";
        pool.Wait();
    }


    {
        cout << "Test #G4: -------------------\n";
        using results_t = std::vector<pair<std::size_t, std::future<std::size_t>>>;
        results_t results;
        for (int z = 0; z < 50; ++z) {
            std::uniform_int_distribution<std::mt19937::result_type> size(10, 30000);
            mtx_.lock();
            int value = size(rng);
            cout << "task #" << z << " posted with value " << value << "\n";
            mtx_.unlock();
            results.emplace_back(std::move(make_pair(z, pool.AddSyncTask(HardTest2, value))));
        }
        pool.AddAsyncTask([&](results_t& r) {
            auto zero = std::chrono::seconds(0);
            std::vector<std::size_t> res_nums(r.size());
            while (!r.empty()) {
                for (auto it = r.begin(); it != r.end();) {
                    if (it->second.wait_for(zero) == std::future_status::ready) {
                        res_nums[it->first] = it->second.get();
                        mtx_.lock();
                        cout << "result #" << it->first << " is ready with value " << res_nums[it->first] << "\n";
                        mtx_.unlock();
                        it = r.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
        }, std::ref(results));
        cout << "waiting results...\n";
        pool.Wait();
    }

    {
        cout << "Test #G5: -------------------\n";
        std::unique_ptr<Task> main_task = std::make_unique<Task>();
        std::vector<std::string> strs;
        std::condition_variable done;
        bool processed{ false };
        strs.push_back("initial str");
        strs.push_back("another initial str");
        cout << "initial content:\n";
        for (const auto& s : strs) {
            cout << s << '\n';
        }
        std::function<int(int, int)> calc = [](const int a, const int b) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return a * b;
        };
        main_task->vars.Add(std::ref(strs));
        main_task->vars.Add(std::move(calc));
        main_task->vars.Add(std::ref(done));
        main_task->SetAsyncJob([](ThreadPool& pool, Task& task, const int count, bool& processed) {
            std::vector<std::string>& strs = task.vars.Get<std::reference_wrapper<std::vector<std::string>>>(0);
            auto fnc = task.vars.Get<std::function<int(int, int)>>(1);
            std::condition_variable& done = task.vars.Get<std::reference_wrapper<std::condition_variable>>(2);
            for (int i = 0; i < count; ++i) {
                int a = RandomN(1, 1000);
                int b = RandomN(1, 1000);
                std::future<int> res = pool.AddSyncTask(fnc, a, b);
                strs.push_back("calculating "s + std::to_string(a) + " * " + std::to_string(b) + " = " + std::to_string(res.get()));
            }
            processed = true;
            done.notify_one();
        }, std::ref(pool), std::ref(*main_task), 20, std::ref(processed));
        pool.AddAsyncTask(std::move(main_task));

        std::unique_ptr<Task> wait_task = std::make_unique<Task>();
        wait_task->vars.Add(std::ref(done));
        wait_task->SetAsyncJob([](Task& task, const std::vector<std::string>& strs, bool& processed) {
            std::condition_variable& done = task.vars.Get<std::reference_wrapper<std::condition_variable>>(0);
            std::mutex mtx;
            std::unique_lock lock(mtx);
            while (!processed) {
                done.wait(lock);
            }
            cout << "processed content:\n";
            for (const auto& s : strs) {
                cout << s << '\n';
            }

        }, std::ref(*wait_task), std::ref(strs), std::ref(processed));

        pool.AddAsyncTask(std::move(wait_task));

        pool.Wait();
    }

    {
        cout << "Test #G5: -------------------\n";
        std::unique_ptr<Task> main_task = std::make_unique<Task>();
        int val{ 10 };
        main_task->vars.Add(std::ref(val));
        main_task->SetAsyncJob([](Task& task, ThreadPool& pool) {
            std::unique_ptr<Task> second_task = std::make_unique<Task>();
            int& val = second_task->vars.Emplace(std::ref(task.vars.Get<std::reference_wrapper<int>>(0)));
            ++val;
            cout << "inside 1 val: " << val << '\n';
            second_task->SetAsyncJob([](Task& task) {
                int& val = task.vars.Get<std::reference_wrapper<int>>(0);
                ++val;
                cout << "inside 2 val: " << val << '\n';
            }, std::ref(*second_task));
            pool.AddAsyncTask(std::move(second_task));
        }, std::ref(*main_task), std::ref(pool));
        pool.AddAsyncTask(std::move(main_task));
        pool.Wait();
        cout << "outside val: " << val << '\n';
    }

}

class Test {
public:
    int val{ 10 };
};

int main() {
    RunTests();
    cout << "Done!" << endl;
}