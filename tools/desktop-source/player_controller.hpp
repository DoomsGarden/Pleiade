#pragma once
#include "math.hpp"
#include "simulation.hpp"

namespace pl {
struct PlayerInput {
    float right = 0, forward = 0;
    bool sprint = false, jump = false;
};

struct WalkSolid {
    V3 center, half;
};

// The ground provider returns terrain height in metres, or a non-finite
// value outside its walkable domain. Solids occupy separate vertical spans.
class PlayerController {
    static constexpr float eyeHeight = 1.7f;
    static constexpr float bodyHeight = 1.9f;
    static constexpr float bodyRadius = .27f;
    static constexpr float maxRampGrade = .84f;
    static constexpr float maxStepHeight = .24f;
    static constexpr float groundTolerance = .0001f;
    V3 feet{}, previousFeet{};
    float verticalSpeed = 0;
    bool onGround = false, previousJump = false;

    static float top(const WalkSolid &solid) { return solid.center.y + solid.half.y; }
    static float bottom(const WalkSolid &solid) { return solid.center.y - solid.half.y; }

    static bool overlapsFootprint(V3 point, const WalkSolid &solid) {
        return std::abs(point.x - solid.center.x) < solid.half.x + bodyRadius - groundTolerance &&
               std::abs(point.z - solid.center.z) < solid.half.z + bodyRadius - groundTolerance;
    }

    static bool overlapsHeight(float y, const WalkSolid &solid) {
        return y < top(solid) - groundTolerance && y + bodyHeight > bottom(solid) + groundTolerance;
    }

    static bool crossesSolid(V3 start, V3 offset, const WalkSolid &solid) {
        // Intersect the path of the feet with the solid expanded by the body.
        float first = 0, last = 1;
        auto clip = [&](float position, float travel, float center, float half) {
            const float extent = half - groundTolerance;
            const float relative = position - center;
            if (std::abs(relative) >= extent + std::abs(travel))
                return false;
            if (travel == 0)
                return true;
            float entry = (-extent - relative) / travel;
            float exit = (extent - relative) / travel;
            if (entry > exit)
                std::swap(entry, exit);
            first = std::max(first, entry);
            last = std::min(last, exit);
            return first < last;
        };
        return clip(start.y, offset.y, solid.center.y - bodyHeight * .5f, solid.half.y + bodyHeight * .5f) &&
               clip(start.x, offset.x, solid.center.x, solid.half.x + bodyRadius) &&
               clip(start.z, offset.z, solid.center.z, solid.half.z + bodyRadius);
    }

    static float supportHeight(V3 point, float terrain, const std::vector<WalkSolid> &solids) {
        for (const auto &solid : solids) {
            const float height = top(solid);
            if (height > terrain && height <= point.y + groundTolerance && overlapsFootprint(point, solid))
                terrain = height;
        }
        return terrain;
    }

  public:
    template <class Ground>
    bool spawn(V3 eyePosition, const Ground &ground, const std::vector<WalkSolid> &solids = {}) {
        const float height = ground(eyePosition.x, eyePosition.z);
        if (!std::isfinite(height))
            return false;
        const V3 start{eyePosition.x, height, eyePosition.z};
        for (const auto &solid : solids)
            if (overlapsHeight(start.y, solid) && overlapsFootprint(start, solid))
                return false;
        feet = previousFeet = start;
        verticalSpeed = 0;
        onGround = true;
        previousJump = false;
        return true;
    }

    template <class Ground>
    void step(PlayerInput input, float yaw, const Ground &ground, const std::vector<WalkSolid> &solids = {}) {
        constexpr float dt = static_cast<float>(FixedSimulation::step);
        constexpr float gravity = 9.81f;
        previousFeet = feet;
        if (input.jump && !previousJump && onGround) {
            verticalSpeed = 4.6f;
            onGround = false;
        }
        previousJump = input.jump;

        const V3 forward{std::sin(yaw), 0, std::cos(yaw)};
        const V3 right = cross(forward, {0, 1, 0});
        V3 move = forward * input.forward + right * input.right;
        const float magnitude = length(move);
        move = move * (dt * (input.sprint ? 6.4f : 3.6f) / std::max(1.f, magnitude));
        float terrain = ground(feet.x, feet.z);

        auto tryMove = [&](V3 offset) {
            V3 next = feet + offset;
            const float height = ground(next.x, next.z);
            const float distance = length(offset);
            const float rise = onGround ? maxRampGrade * distance : 0.f;
            if (!std::isfinite(height))
                return false;
            const float difference = height - feet.y;
            if (difference > rise + groundTolerance) {
                if (!onGround || difference > maxStepHeight || distance < groundTolerance)
                    return false;
                // A low riser can be climbed when its upper tread is walkable.
                // Sampling the tread also rejects a continuous steep slope.
                const float probeDistance = .025f;
                const auto probe = next + unit(offset) * probeDistance;
                const float ahead = ground(probe.x, probe.z);
                if (!std::isfinite(ahead) ||
                    std::abs(ahead - height) > maxRampGrade * probeDistance + groundTolerance)
                    return false;
                next.y = height;
            }
            // A shallow block can act as a tread. A higher one remains a wall.
            if (onGround) {
                for (const auto &solid : solids)
                    if (overlapsHeight(next.y, solid) && top(solid) - feet.y <= maxStepHeight + groundTolerance &&
                        overlapsFootprint(next, solid))
                        next.y = top(solid);
            }
            V3 sweepStart = feet;
            if (onGround && next.y == feet.y && std::abs(difference) <= rise + groundTolerance &&
                std::abs(feet.y - terrain) <= groundTolerance) {
                // Follow an attached terrain grade during the sweep. A
                // tread rises first, and a ledge keeps its height until falling.
                offset.y = difference;
            } else
                sweepStart.y = std::max(next.y, height);
            for (const auto &solid : solids)
                if (crossesSolid(sweepStart, offset, solid))
                    return false;
            feet = next;
            terrain = height;
            return true;
        };
        if (magnitude > 0 && !tryMove(move) && move.x != 0 && move.z != 0) {
            // An obstructed diagonal can still move along either axis.
            V3 x{move.x, 0, 0}, z{0, 0, move.z};
            if (std::abs(move.z) > std::abs(move.x))
                std::swap(x, z);
            if (!tryMove(x))
                tryMove(z);
        }

        const float floor = supportHeight(feet, terrain, solids);
        // Stay attached to gentle downhill grades. A ledge puts the
        // character into free fall until the next terrain contact.
        const float travel = length(feet - previousFeet);
        if (onGround && std::isfinite(floor) &&
            std::abs(floor - feet.y) <= maxRampGrade * travel + groundTolerance) {
            feet.y = floor;
            return;
        }
        onGround = false;
        float nextY = feet.y + (verticalSpeed - gravity * dt * .5f) * dt;
        verticalSpeed -= gravity * dt;
        if (nextY > feet.y) {
            const float head = feet.y + bodyHeight;
            for (const auto &solid : solids) {
                const float ceiling = bottom(solid);
                if (ceiling >= head - groundTolerance && ceiling < nextY + bodyHeight &&
                    overlapsFootprint(feet, solid)) {
                    nextY = std::max(feet.y, ceiling - bodyHeight);
                    verticalSpeed = 0;
                }
            }
        }
        feet.y = nextY;
        if (verticalSpeed <= 0 && std::isfinite(floor) && feet.y <= floor) {
            feet.y = floor;
            verticalSpeed = 0;
            onGround = true;
        }
    }

    V3 position() const { return feet; }
    bool grounded() const { return onGround; }
    V3 eye(float interpolation = 1) const {
        return previousFeet + (feet - previousFeet) * std::clamp(interpolation, 0.f, 1.f) +
               V3{0, eyeHeight, 0};
    }
};
} // namespace pl
