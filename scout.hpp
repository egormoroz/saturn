#ifndef SCOUT_HPP
#define SCOUT_HPP

#include <mutex>
#include <iostream>

struct CoutWrapper {
    CoutWrapper(std::mutex &mtx) 
        : lock_(mtx) {}

    template<typename T>
    CoutWrapper& operator<<(T && t) {
        std::cout << t;
        return *this;
    }

    ~CoutWrapper() {
        std::cout.flush();
    }

private:
    std::lock_guard<std::mutex> lock_;
};

inline CoutWrapper sync_cout() {
    static std::mutex mutex;
    return CoutWrapper(mutex);
}

#endif
