#pragma once
#include "math.hpp"
#include <cstdint>
#include <limits>
#include <optional>

namespace pl {
struct StrategyRay {
    V3 origin, direction;
    float nearDistance, farDistance;
};

struct StrategyPickTarget {
    std::uint64_t id;
    V3 center, half;
    int priority = 0;
};

struct StrategyPickHit {
    V3 point;
    float distance;
    std::optional<std::uint64_t> entity;
};

// NDC coordinates have +Y up. Match the renderer's perspective and orbit.
inline std::optional<StrategyRay> strategyCursorRay(const Camera &camera, float ndcX, float ndcY,
                                                     float aspect) {
    if (!camera.strategy || !(std::abs(ndcX) <= 1.f && std::abs(ndcY) <= 1.f) ||
        !std::isfinite(aspect) || !(aspect > 0.f))
        return std::nullopt;

    const M4 view = camera.view(), projection = camera.projection(aspect);
    const V3 right{view.at(0, 0), view.at(0, 1), view.at(0, 2)};
    const V3 up{view.at(1, 0), view.at(1, 1), view.at(1, 2)};
    const V3 forward{-view.at(2, 0), -view.at(2, 1), -view.at(2, 2)};
    const V3 direction = unit(forward + right * (ndcX / projection.at(0, 0)) +
                             up * (ndcY / projection.at(1, 1)));
    const float projected = dot(forward, direction);
    if (!(projected > 0.f))
        return std::nullopt;

    const float z = projection.at(2, 2), w = projection.at(2, 3);
    return StrategyRay{camera.eye(), direction, w / ((z - 1.f) * projected),
                       w / ((z + 1.f) * projected)};
}

namespace strategy_detail {
inline std::optional<float> hitBounds(const StrategyRay &ray, const StrategyPickTarget &target,
                                      float limit) {
    const float origin[]{ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[]{ray.direction.x, ray.direction.y, ray.direction.z};
    const float center[]{target.center.x, target.center.y, target.center.z};
    const float half[]{target.half.x, target.half.y, target.half.z};
    float entry = ray.nearDistance, exit = limit;
    for (int axis = 0; axis < 3; ++axis) {
        if (!(half[axis] >= 0.f && std::isfinite(half[axis]) && std::isfinite(center[axis])))
            return std::nullopt;
        const float lo = center[axis] - half[axis], hi = center[axis] + half[axis];
        if (direction[axis] == 0.f) {
            if (origin[axis] < lo || origin[axis] > hi)
                return std::nullopt;
            continue;
        }
        float a = (lo - origin[axis]) / direction[axis];
        float b = (hi - origin[axis]) / direction[axis];
        if (a > b)
            std::swap(a, b);
        entry = std::max(entry, a);
        exit = std::min(exit, b);
        if (entry > exit)
            return std::nullopt;
    }
    return entry;
}

template <class HeightSampler>
std::optional<float> hitTerrain(const StrategyRay &ray, HeightSampler &height, float limit) {
    auto gap = [&](float distance) {
        const V3 point = ray.origin + ray.direction * distance;
        return point.y - height(point.x, point.z);
    };

    // Find the first crossing of a heightfield sampled every half metre,
    // then refine within that interval. Non-finite heights denote holes.
    constexpr float step = .5f;
    float previous = ray.nearDistance, previousGap = gap(previous);
    if (previousGap == 0.f)
        return previous;
    while (previous < limit) {
        const float distance = std::min(previous + step, limit);
        const float currentGap = gap(distance);
        if (currentGap == 0.f)
            return distance;
        if (std::isfinite(previousGap) && std::isfinite(currentGap) &&
            ((previousGap > 0.f) != (currentGap > 0.f))) {
            float lo = previous, hi = distance;
            bool valid = true;
            for (int iteration = 0; iteration < 12; ++iteration) {
                const float mid = (lo + hi) * .5f, midGap = gap(mid);
                if (!std::isfinite(midGap)) {
                    valid = false;
                    break;
                }
                if ((previousGap > 0.f) == (midGap > 0.f))
                    lo = mid;
                else
                    hi = mid;
            }
            if (valid)
                return (lo + hi) * .5f;
        }
        previous = distance;
        previousGap = currentGap;
    }
    return std::nullopt;
}
} // namespace strategy_detail

template <class HeightSampler>
std::optional<StrategyPickHit> pickStrategy(const Camera &camera, float ndcX, float ndcY,
                                           float aspect, const std::vector<StrategyPickTarget> &targets,
                                           HeightSampler &&height,
                                           float maxDistance = std::numeric_limits<float>::infinity()) {
    const auto ray = strategyCursorRay(camera, ndcX, ndcY, aspect);
    if (!ray || !(maxDistance >= ray->nearDistance))
        return std::nullopt;

    float distance = std::min(ray->farDistance, maxDistance);
    const StrategyPickTarget *chosen = nullptr;
    for (const auto &target : targets) {
        const auto hit = strategy_detail::hitBounds(*ray, target, distance);
        if (!hit)
            continue;
        if (!chosen || *hit < distance || target.priority > chosen->priority ||
            (target.priority == chosen->priority && target.id < chosen->id)) {
            distance = *hit;
            chosen = &target;
        }
    }

    const auto ground = strategy_detail::hitTerrain(*ray, height, distance);
    if (ground && (!chosen || *ground < distance))
        return StrategyPickHit{ray->origin + ray->direction * *ground, *ground, std::nullopt};
    if (chosen)
        return StrategyPickHit{ray->origin + ray->direction * distance, distance, chosen->id};
    return std::nullopt;
}
} // namespace pl
