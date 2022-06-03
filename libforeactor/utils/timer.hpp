#include <unordered_map>
#include <vector>
#include <time.h>

#include "debug.hpp"


#pragma once


//////////////////
// Timer macros //
//////////////////

// Timer routines are not active if NTIMER is defined at compilation time
// by the build system.
#ifdef NTIMER

#define TIMER_START(timer)
#define TIMER_PAUSE(timer)
#define TIMER_PRINT(timer, unit)
#define TIMER_RESET(timer)

#else

#define TIMER_START(timer)       timer.Start()
#define TIMER_PAUSE(timer)       timer.Pause()
#define TIMER_PRINT(timer, unit) timer.ShowStat(unit)
#define TIMER_RESET(timer)       timer.Reset()

#endif


////////////////////////////////////
// Timers internal implementation //
////////////////////////////////////

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


}
