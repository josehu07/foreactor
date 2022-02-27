#include <unordered_map>
#include <vector>
#include <chrono>


#ifndef __FOREACTOR_TIMER_H__
#define __FOREACTOR_TIMER_H__


// timer routines are not active if NTIMER is defined
#ifdef NTIMER

#define TIMER_START(id)
#define TIMER_PAUSE(id)
#define TIMER_PRINT_ALL(unit)
#define TIMER_RESET_ALL()

#else

#define TIMER_START(id)                          \
    do {                                         \
        if (timers.find(id) == timers.end())     \
            timers.emplace((id), (id));          \
        timers.at(id).Start();                   \
    } while (0)

#define TIMER_PAUSE(id)                          \
    do {                                         \
        assert(timers.find(id) != timers.end()); \
        timers.at(id).Pause();                   \
    } while (0)

#define TIMER_PRINT_ALL(unit)           \
    do {                                \
        for (auto& [_, timer] : timers) \
            timer.ShowStat(unit);       \
    } while (0)

#define TIMER_RESET_ALL()               \
    do {                                \
        for (auto& [_, timer] : timers) \
            timer.Reset();              \
    } while (0)

#endif


namespace foreactor {


typedef enum TimeUnit {
    TIME_NANO,
    TIME_MICRO,
    TIME_MILLI,
    TIME_SEC
} TimeUnit;

class Timer {
    private:
        std::string id;
        std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> start_tps;
        std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> pause_tps;

    public:
        Timer() = delete;
        Timer(std::string id) : id(id) {}
        ~Timer() {}

        void Reset();

        void Start();
        void Pause();

        size_t GetSize() const;
        std::vector<double> GetStat(TimeUnit unit) const;

        void ShowStat(TimeUnit unit) const;
};


// collection of timers
extern thread_local std::unordered_map<std::string, Timer> timers;


}


#endif
