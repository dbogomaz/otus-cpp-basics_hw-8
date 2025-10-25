#ifndef TIMER_H
#define TIMER_H

#include <chrono>

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

class Timer {
public:
    Timer() : m_startPoint(std::chrono::high_resolution_clock::now()) {}

    ~Timer() { stop(); }

    void stop() {
        auto endPoint = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(endPoint - m_startPoint).count();
        printf("Elapsed time: %lld ms (%.3f s)\n", duration, static_cast<double>(duration) / 1000);
    }

private:
    TimePoint m_startPoint;
};

#endif  // TIMER_H