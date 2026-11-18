#pragma once
#include <algorithm>
#include <stdexcept>

namespace pl {
// Times are monotonic seconds relative to the beginning of the render loop.
// Start measurement on a frame boundary, after the full warmup has elapsed.
struct BenchmarkClock {
    double warmupSeconds, durationSeconds, measurementStart = -1;
    BenchmarkClock(double warmup, double duration) : warmupSeconds(warmup), durationSeconds(duration) {
        if (warmup < 0 || duration <= 0)
            throw std::invalid_argument("Invalid benchmark duration");
    }
    bool beginFrame(double elapsed) {
        if (measurementStart < 0 && elapsed >= warmupSeconds)
            measurementStart = elapsed;
        return measurementStart >= 0;
    }
    bool complete(double elapsed) const {
        return measurementStart >= 0 && elapsed - measurementStart >= durationSeconds;
    }
    double measuredSeconds(double elapsed) const {
        return measurementStart < 0 ? 0 : std::max(0.0, elapsed - measurementStart);
    }
};
} // namespace pl
