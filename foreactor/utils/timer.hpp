#include <unordered_map>
#include <vector>
#include <time.h>

#include "debug.hpp"


#ifndef __FOREACTOR_TIMER_H__
#define __FOREACTOR_TIMER_H__


//////////////////
// Timer macros //
//////////////////

// Timer routines are not active if NTIMER is defined at compilation time
// by the build system.
#ifdef NTIMER

#define TIMER_START(id)
#define TIMER_PAUSE(id)
#define TIMER_PRINT(id, unit)
#define TIMER_RESET(id)
#define TIMER_CLEAR(id)

#else

#define TIMER_START(id)                                   \
    do {                                                  \
        std::string uid = std::to_string(tid) + "-" + id; \
        if (timers.find(uid) == timers.end())             \
            timers.emplace((uid), (uid));                 \
        timers.at(uid).Start();                           \
    } while (0)

#define TIMER_PAUSE(id)                                   \
    do {                                                  \
        std::string uid = std::to_string(tid) + "-" + id; \
        assert(timers.find(uid) != timers.end());         \
        timers.at(uid).Pause();                           \
    } while (0)

#define TIMER_PRINT(id, unit)                             \
    do {                                                  \
        std::string uid = std::to_string(tid) + "-" + id; \
        if (timers.find(uid) != timers.end())             \
            timers.at(uid).ShowStat(unit);                \
    } while (0)

#define TIMER_RESET(id)                                   \
    do {                                                  \
        std::string uid = std::to_string(tid) + "-" + id; \
        if (timers.find(uid) != timers.end())             \
            timers.at(uid).Reset();                       \
    } while (0)

#define TIMER_CLEAR(id)                                   \
    do {                                                  \
        std::string uid = std::to_string(tid) + "-" + id; \
        if (timers.find(uid) != timers.end())             \
            timers.erase(uid);                            \
    } while (0)

#endif


//////////////////////////////////
// Timers internally use chrono //
//////////////////////////////////

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

        struct timespec start_ts;
        struct timespec pause_ts;
        bool started = false;

        std::vector<uint64_t> nanosecs;

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


// collection of timers created
extern std::unordered_map<std::string, Timer> timers;


}


#endif
