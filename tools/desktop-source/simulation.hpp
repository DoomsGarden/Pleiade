#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace pl {
class FixedSimulation {
    double accumulator = 0;
    uint64_t ticks = 0;

  public:
    static constexpr double step = 1.0 / 60.0;
    uint64_t dropped = 0;
    int advance(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0)
            throw std::invalid_argument("Invalid simulation delta");
        accumulator += seconds;
        int executed = 0;
        while (accumulator + 1e-10 >= step && executed < 8) {
            accumulator -= step;
            ++ticks;
            ++executed;
        }
        if (accumulator >= step) {
            auto skipped = static_cast<uint64_t>(std::floor(accumulator / step));
            dropped += skipped;
            accumulator -= skipped * step;
        }
        accumulator = std::max(0.0, accumulator);
        return executed;
    }
    double time() const { return ticks * step; }
    double renderTime() const { return time() + accumulator; }
};
} // namespace pl
