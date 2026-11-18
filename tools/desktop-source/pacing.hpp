#pragma once
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pl {
// Absolute deadlines retain cadence across small scheduler delays. Large misses
// restart the schedule instead of producing a burst of catch-up frames.
struct FrameSchedule {
    using Clock = std::chrono::steady_clock;
    Clock::duration period;
    Clock::time_point deadline{};
    bool initialized = false;
    explicit FrameSchedule(int fps)
        : period(fps > 0 ? std::chrono::duration_cast<Clock::duration>(
                               std::chrono::duration<double>(1.0 / fps))
                         : Clock::duration::zero()) {}
    Clock::time_point target(Clock::time_point start) {
        if (!initialized) {
            deadline = start + period;
            initialized = true;
        }
        return deadline;
    }
    bool advance(Clock::time_point wake) {
        bool reset = wake - deadline >= period / 2;
        deadline = reset ? wake + period : deadline + period;
        return reset;
    }
    void reset() { initialized = false; }
};

class FramePacer {
    FrameSchedule schedule;
    bool legacy;
#ifdef _WIN32
    HANDLE timer = nullptr;
#endif
    const char *backendName = "steady-clock-sleep";

  public:
    int resyncs = 0, waitFailures = 0;
    FramePacer(int fps, bool old = false) : schedule(fps), legacy(old) {
        if (fps == 0)
            backendName = "uncapped";
        else if (old)
            backendName = "legacy-relative-sleep";
#ifdef _WIN32
        else {
            timer = CreateWaitableTimerExW(nullptr, nullptr, 0x2, TIMER_MODIFY_STATE | SYNCHRONIZE);
            if (timer)
                backendName = "windows-high-resolution-timer";
            else {
                timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
                if (timer)
                    backendName = "windows-waitable-timer";
            }
        }
#endif
    }
    ~FramePacer() {
#ifdef _WIN32
        if (timer)
            CloseHandle(timer);
#endif
    }
    FramePacer(const FramePacer &) = delete;
    FramePacer &operator=(const FramePacer &) = delete;
    const char *backend() const { return backendName; }
    void reset() { schedule.reset(); }
    void wait(FrameSchedule::Clock::time_point start) {
        using Clock = FrameSchedule::Clock;
        if (schedule.period == Clock::duration::zero())
            return;
        auto deadline = legacy ? start + schedule.period : schedule.target(start);
#ifdef _WIN32
        if (timer && !legacy) {
            auto remaining = deadline - Clock::now();
            if (remaining > Clock::duration::zero()) {
                LARGE_INTEGER due;
                due.QuadPart =
                    -(std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() / 100 + 1);
                if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) ||
                    WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
                    ++waitFailures;
            }
        }
#endif
        // Also handles early wakes and native API failure without spinning.
        std::this_thread::sleep_until(deadline);
        if (!legacy && schedule.advance(Clock::now()))
            ++resyncs;
    }
};
} // namespace pl
