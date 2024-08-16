#include <numeric>
#include <string>
#include <thread>
#include <cstddef>
#include <chrono>
#include <iostream>
#include <random>
#include <mutex>
#include <threadpool.hpp>
#include <log_duration.hpp>

using namespace std;
using namespace vsock;
using namespace chrono;

std::string FormatThousands(std::size_t value) {
    std::string result = std::to_string(value);
    for (int i = result.size() - 3; i > 0; i -= 3) {
        result.insert(i, ",");
    }
    return result;
}

static std::mutex mtx_;

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

void PrintTask(int id) {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist6(1, 5);
    int time = dist6(rng);

    std::this_thread::sleep_for(std::chrono::seconds(time));
    mtx_.lock();
    cout << "PrintTask: task# " << id << " sleep " << time << endl;
    mtx_.unlock();
}

void PrintTask2() {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist6(1, 5);
    int time = dist6(rng);

    std::this_thread::sleep_for(std::chrono::seconds(time));
    mtx_.lock();
    cout << "PrintTask2: " << " sleep " << time << endl;
    mtx_.unlock();
}

int main() {

    int avg_rounds{ 3 };
    int task_count{ 10 };
    int test_size{ 1000 };
    int thread_count{ 32 };

    cout << "\n=========================================================================\n"s;



    {
        ThreadPool pool(thread_count);
        std::size_t avg{ 0 };
        for (int z = 0; z < avg_rounds; ++z) {
            LogDuration log("test1", false);
            //cout << "ROUND#" << (z + 1) << " --------------\n";
            for (int i = 0; i < task_count; ++i) {
                pool.AddAsyncTask([&](Task& task) {HardTest1(test_size);});
                pool.AddAsyncTask([&](Task& task) {PrintTask2();});
                pool.AddAsyncTask([=](Task& task) {PrintTask(i);});
                auto res = pool.AddSyncTask([&]() -> std::size_t {return HardTest2(test_size);});
                cout << "\t[" << i << "] = " << std::to_string(res.get()) << '\n';
            }
            std::size_t time = log.GetTime();
            //cout << "ROUND#" << (z + 1) << ": " << FormatThousands(time) << " ns" << "\n";
            avg += time;

        }
        avg /= avg_rounds;
        cout << "--------------------------------------\n"s;
        cout << "test1:\t\t\t" << FormatThousands(avg) << " ns" << endl;
    }


    {
        ThreadPool pool(thread_count);

        pool.AddAsyncTask([](Task& task) {
            cout << "Begin" << endl;
        });

        for (int i = 0; i < 32; ++i) {

            Task task;
            task.AddVariables(0);
            task.SetConditionFunction([](Task& task) {
                return std::any_cast<int>(task.GetVariableRef(0)) < 10;
            });
            task.SetTaskFuncion([](Task& task) {
                int& counter = std::any_cast<int&>(task.GetVariableRef(0));
                std::this_thread::sleep_for(std::chrono::seconds(1));

                cout << "counter = " << counter << '\n';
                ++counter;
            });
            pool.AddAsyncTask(std::move(task));

        }

        pool.AddAsyncTask([](Task& task) {
            cout << "Hello" << endl;
        });
    }


}