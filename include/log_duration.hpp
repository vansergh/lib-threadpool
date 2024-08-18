#include <chrono>
#include <iostream>
#include <string>
#include <cstddef>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // LogDuration class defenition
    ////////////////////////////////////////////////////////////////////////////////

    class LogDuration {
    public:

        using Clock = std::chrono::steady_clock;

        LogDuration(const std::string& id, bool print = true) : id_{ id }, print_{ print } {
        }

        std::size_t GetTime(bool print = false) {
            using namespace std::chrono;
            using namespace std::literals;

            const auto end_time = Clock::now();
            const auto dur = end_time - start_time_;
            std::size_t res = duration_cast<nanoseconds>(dur).count();
            if (print) {
                std::cerr << id_ << ": "s << FormatThousands_(res) << " ns"s << std::endl;
            }
            return res;
        }

        ~LogDuration() {
            GetTime(print_);
        }

    private:

        std::string FormatThousands_(std::size_t value) {
            std::string result = std::to_string(value);
            for (int i = result.size() - 3; i > 0; i -= 3) {
                result.insert(i, ",");
            }
            return result;
        }

        const std::string id_;
        bool print_;
        const Clock::time_point start_time_ = Clock::now();
    };
}