#pragma once
#include "math.hpp"
#include "simulation.hpp"

namespace pl {
struct PlayerInput {
    float right = 0, forward = 0;
    bool sprint = false, jump = false;
};

// A terrain controller in metres. The ground provider returns the height at
// (x, z), or a non-finite value outside its walkable domain.
class PlayerController {
    static constexpr float eyeHeight = 1.7f;
    static constexpr float maxRampGrade = .84f;
    static constexpr float maxStepHeight = .24f;
    static constexpr float groundTolerance = .0001f;
    V3 feet{}, previousFeet{};
    float verticalSpeed = 0;
    bool onGround = false, previousJump = false;

  public:
    template <class Ground> bool spawn(V3 eyePosition, const Ground &ground) {
        const float height = ground(eyePosition.x, eyePosition.z);
        if (!std::isfinite(height))
            return false;
        feet = previousFeet = {eyePosition.x, height, eyePosition.z};
        verticalSpeed = 0;
        onGround = true;
        previousJump = false;
        return true;
    }

    template <class Ground> void step(PlayerInput input, float yaw, const Ground &ground) {
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
        float floor = ground(feet.x, feet.z);

        auto tryMove = [&](V3 offset) {
            V3 next = feet + offset;
            const float height = ground(next.x, next.z);
            const float rise = onGround ? maxRampGrade * length(offset) : 0.f;
            if (!std::isfinite(height))
                return false;
            const float difference = height - feet.y;
            if (difference > rise + groundTolerance) {
                if (!onGround || difference > maxStepHeight || length(offset) < groundTolerance)
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
            feet = next;
            floor = height;
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

        // Stay attached to gentle downhill grades. A ledge puts the
        // character into free fall until the next terrain contact.
        const float travel = length(feet - previousFeet);
        if (onGround && std::isfinite(floor) &&
            std::abs(floor - feet.y) <= maxRampGrade * travel + groundTolerance) {
            feet.y = floor;
            return;
        }
        onGround = false;
        feet.y += (verticalSpeed - gravity * dt * .5f) * dt;
        verticalSpeed -= gravity * dt;
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
