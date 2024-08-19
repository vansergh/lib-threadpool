#include <memory>
#include <functional>

class Node {
public:
    Node(Node&&) = delete;
    Node(const Node&) = delete;
    Node& operator=(Node&&) = delete;
    Node& operator=(const Node&) = delete;
public:

    Node() = default;

    template<typename T>
    Node(T&& data) : data_{ Put_(std::move(data)) } {}

    template<typename T>
    void Put(T&& data) {
        Drop();
        data_ = Put_(std::move(data));
    }

    template<typename T>
    T& Get() {
        return *(reinterpret_cast<T*>(data_));
    }

    void Drop() {
        if (!data_) {
            return;
        }
        delete_fnc(data_);
        data_ = nullptr;
    }

    ~Node() {
        Drop();
    }

private:

    template<typename T>
    void* Put_(T&& data) {
        if constexpr (std::is_array_v<T[]>) {
            return nullptr;
        }
        void* result = reinterpret_cast<void*>(new T(std::move(data)));
        delete_fnc = Node::Delete_<T>;
        return result;
    }

    template<typename T>
    static void Delete_(void* ptr) {
        T* rptr = reinterpret_cast<T*>(ptr);
        if constexpr (std::is_array_v<T>) {
            delete[] rptr;
        }
        else {
            delete rptr;
        }
        ptr = nullptr;
    }

private:
    void* data_{ nullptr };
    void (*delete_fnc)(void*) { nullptr };
};
