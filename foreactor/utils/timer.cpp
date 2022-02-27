#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <assert.h>
#include <stdio.h>

#include "timer.hpp"


namespace foreactor {


thread_local std::unordered_map<std::string, Timer> timers;


static std::string TimeUnitStr(TimeUnit unit) {
    switch (unit) {
        case TIME_NANO:  return "ns";
        case TIME_MICRO: return "us";
        case TIME_MILLI: return "ms";
        case TIME_SEC:   return "s";
        default:         return "unknown_unit";
    }
}


void Timer::Reset() {
    start_tps.clear();
    pause_tps.clear();
}


void Timer::Start() {
    assert(start_tps.size() == pause_tps.size());
    start_tps.push_back(std::chrono::high_resolution_clock::now());
}

void Timer::Pause() {
    assert(start_tps.size() == pause_tps.size() + 1);
    pause_tps.push_back(std::chrono::high_resolution_clock::now());
}


size_t Timer::GetSize() const {
    assert(start_tps.size() == pause_tps.size());
    return start_tps.size();
}

std::vector<double> Timer::GetStat(TimeUnit unit) const {
    assert(start_tps.size() == pause_tps.size());

    std::vector<double> stat;
    for (size_t i = 0; i < start_tps.size(); ++i) {
        std::chrono::duration<double, std::nano>  elapsed_ns;
        std::chrono::duration<double, std::micro> elapsed_us;
        std::chrono::duration<double, std::milli> elapsed_ms;
        std::chrono::duration<double>             elapsed_secs;

        switch (unit) {
            case TIME_NANO:
                elapsed_ns = pause_tps[i] - start_tps[i];
                stat.push_back(elapsed_ns.count());
                break;
            case TIME_MICRO:
                elapsed_us = pause_tps[i] - start_tps[i];
                stat.push_back(elapsed_us.count());
                break;
            case TIME_MILLI:
                elapsed_ms = pause_tps[i] - start_tps[i];
                stat.push_back(elapsed_ms.count());
                break;
            case TIME_SEC:
                elapsed_secs = pause_tps[i] - start_tps[i];
                stat.push_back(elapsed_secs.count());
                break;
            default:
                assert(false);
        }
    }

    return stat;
}


void Timer::ShowStat(TimeUnit unit) const {
    std::vector<double> stat = GetStat(unit);

    if (stat.size() > 0) {
        double sum = 0., max = std::numeric_limits<double>::min(),
                         min = std::numeric_limits<double>::max();
        for (double& time : stat) {
            sum += time;
            max = time > max ? time : max;
            min = time < min ? time : min;
        }
        double avg = sum / stat.size();
        fprintf(stderr, "# %-20s #  cnt %5lu  "
                        "avg %8.3lf  max %8.3lf  min %8.3lf  %s\n",
               id.c_str(), stat.size(), avg, max, min,
               TimeUnitStr(unit).c_str());
    } else {
        fprintf(stderr, "# %20s #  cnt 0\n", id.c_str());
    }
}


}
