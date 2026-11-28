#pragma once
#include "math.hpp"
#include <utility>

namespace pl {
struct CinematicKey {
    double time;
    V3 position;
    V3 target;
    float verticalFovDegrees = 62.f;
    bool cut = false;
};

struct CinematicPose {
    V3 position;
    V3 target;
    float verticalFovDegrees;
};

// Times are in seconds. A cut holds the preceding pose until its own timestamp.
class CinematicTrack {
    std::vector<CinematicKey> keys;

    static bool finite(V3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }
    static CinematicPose pose(const CinematicKey &key) {
        return {key.position, key.target, key.verticalFovDegrees};
    }
    static bool validAim(V3 position, V3 target) {
        V3 aim = target - position;
        float squaredDistance = dot(aim, aim);
        return squaredDistance > 1e-10f && std::isfinite(squaredDistance);
    }
    static double tangent(double incoming, double outgoing) {
        if ((incoming > 0 && outgoing > 0) || (incoming < 0 && outgoing < 0)) {
            double a = std::abs(incoming), b = std::abs(outgoing);
            // Limit shared slopes at extrema and on intervals of unequal length.
            return std::copysign(std::min(.5 * (a + b), 2 * std::min(a, b)), incoming);
        }
        return 0;
    }

  public:
    explicit CinematicTrack(std::vector<CinematicKey> keyframes) : keys(std::move(keyframes)) {
        if (keys.empty())
            throw std::invalid_argument("Cinematic track requires a keyframe");
        double previous = -1;
        for (const auto &key : keys) {
            if (!std::isfinite(key.time) || key.time < 0 || key.time <= previous)
                throw std::invalid_argument("Cinematic keyframe times must increase");
            if (!finite(key.position) || !finite(key.target) || !validAim(key.position, key.target))
                throw std::invalid_argument("Cinematic keyframe requires a valid camera and target");
            if (!(key.verticalFovDegrees > 0 && key.verticalFovDegrees < 180))
                throw std::invalid_argument("Cinematic vertical field of view must be between 0 and 180 degrees");
            previous = key.time;
        }
    }

    double duration() const { return keys.back().time; }

    CinematicPose sample(double time) const {
        if (!std::isfinite(time))
            throw std::invalid_argument("Cinematic sample time must be finite");
        auto next = std::upper_bound(keys.begin(), keys.end(), time,
                                     [](double seconds, const CinematicKey &key) {
                                         return seconds < key.time;
                                     });
        if (next == keys.begin())
            return pose(keys.front());
        if (next == keys.end())
            return pose(keys.back());
        auto current = next - 1;
        if (next->cut || time == current->time)
            return pose(*current);

        auto previous = current == keys.begin() || current->cut ? current : current - 1;
        auto following = next + 1;
        if (following == keys.end() || following->cut)
            following = next;
        double interval = next->time - current->time;
        double u = (time - current->time) / interval;
        double blend = u * u * (3 - 2 * u);
        double startWeight = u * (1 - u) * (1 - u), endWeight = u * u * (u - 1);
        auto channel = [&](double before, double start, double end, double after) {
            double movement = end - start;
            double startTangent = previous == current
                                      ? 0
                                      : tangent((start - before) / (current->time - previous->time) * interval,
                                                movement);
            double endTangent = following == next
                                    ? 0
                                    : tangent(movement, (after - end) / (following->time - next->time) * interval);
            return float(start + movement * blend + startTangent * startWeight + endTangent * endWeight);
        };
        auto vector = [&](V3 before, V3 start, V3 end, V3 after) {
            return V3{channel(before.x, start.x, end.x, after.x),
                      channel(before.y, start.y, end.y, after.y),
                      channel(before.z, start.z, end.z, after.z)};
        };
        CinematicPose result{vector(previous->position, current->position, next->position, following->position),
                             vector(previous->target, current->target, next->target, following->target),
                             channel(previous->verticalFovDegrees, current->verticalFovDegrees,
                                     next->verticalFovDegrees, following->verticalFovDegrees)};
        // Preserve a usable view if the camera path passes through its focus.
        if (!validAim(result.position, result.target)) {
            const auto &key = u < .5 ? *current : *next;
            result.target = result.position + (key.target - key.position);
        }
        return result;
    }
};
} // namespace pl
