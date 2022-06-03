#include <string>
#include <unordered_map>
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "timer.hpp"


namespace foreactor {


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
    nanosecs.clear();
    started = false;
}


void Timer::Start() {
    assert(!started);
    [[maybe_unused]] int ret = clock_gettime(CLOCK_MONOTONIC, &start_ts);
    assert(ret == 0);
    started = true;
}

void Timer::Pause() {
    assert(started);
    [[maybe_unused]] int ret = clock_gettime(CLOCK_MONOTONIC, &pause_ts);
    assert(ret == 0);
    started = false;

    uint64_t nsecs = (pause_ts.tv_sec - start_ts.tv_sec) * 1e9
                     + (pause_ts.tv_nsec - start_ts.tv_nsec);
    nanosecs.push_back(nsecs);
}


size_t Timer::GetSize() const {
    assert(!started);
    return nanosecs.size();
}

std::vector<double> Timer::GetStat(TimeUnit unit) const {
    assert(!started);
    double div = 1;
    switch (unit) {
        case TIME_NANO:
            div = 1;
            break;
        case TIME_MICRO:
            div = 1e3;
            break;
        case TIME_MILLI:
            div = 1e6;
            break;
        case TIME_SEC:
            div = 1e9;
            break;
        default:
            assert(false);
    }

    std::vector<double> stat;
    stat.reserve(nanosecs.size());
    for (uint64_t nsecs : nanosecs)
        stat.push_back(static_cast<double>(nsecs) / div);

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
        fprintf(stderr, "# %-24s #  cnt %5lu  "
                        "avg %10.3lf  max %10.3lf  min %10.3lf  sum %10.3lf  "
                        "%s\n",
               id.c_str(), stat.size(), avg, max, min, sum,
               TimeUnitStr(unit).c_str());
    } else
        fprintf(stderr, "# %-24s #  cnt %5lu\n", id.c_str(), 0lu);
}


}
