#include "../tools/desktop-source/player_controller.hpp"
#include <iostream>
#include <limits>

using namespace pl;

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void expectNear(float actual, float expected, const char *message) {
    require(std::abs(actual - expected) < .001f, message);
}
constexpr auto flat = [](float, float) { return 0.f; };

V3 walk(PlayerInput input, int frames, float yaw = 0) {
    PlayerController player;
    require(player.spawn({0, 4, 0}, flat), "flat-world spawn");
    FixedSimulation simulation;
    for (int frame = 0; frame < frames; ++frame) {
        const int steps = simulation.advance(1. / frames);
        for (int i = 0; i < steps; ++i)
            player.step(input, yaw, flat);
    }
    return player.position();
}

void movement() {
    expectNear(walk({0, 1}, 30).z, 3.6f, "walking speed");
    expectNear(walk({0, .5f}, 30).z, 1.8f, "analogue movement sticks to requested speed");
    expectNear(walk({0, 1, true}, 30).z, 6.4f, "sprint speed");
    expectNear(walk({0, 1}, 48, pi / 2).x, 3.6f, "motion follows camera yaw");
    expectNear(length(walk({1, 1}, 60)), 3.6f, "diagonal motion stays normalized");
    expectNear(length(walk({1, 1}, 30) - walk({1, 1}, 144)), 0, "stable across frame rates");
}

void jumping() {
    PlayerController player;
    player.spawn({0, 5, 0}, flat);
    expectNear(player.eye().y, 1.7f, "camera is at standing eye height");
    player.step({0, 1, false, true}, 0, flat);
    require(!player.grounded() && player.position().y > 0, "jump leaves the surface");
    expectNear(player.eye(0).y, 1.7f, "interpolation at previous tick");
    expectNear(player.eye(.5f).y, (1.7f + player.eye().y) * .5f, "interpolation between ticks");
    float peak = player.position().y;
    for (int i = 0; i < 90; ++i) {
        player.step({0, 1, false, true}, 0, flat);
        peak = std::max(peak, player.position().y);
    }
    require(peak > 1.f && peak < 1.1f, "jump reaches its ballistic apex");
    require(player.grounded() && player.position().y == 0, "hold jump does not bounce after landing");
    player.step({}, 0, flat);
    player.step({0, 0, false, true}, 0, flat);
    require(!player.grounded(), "a new press jumps after landing");
}

void terrain() {
    PlayerController player;
    const auto ramp = [](float, float z) { return z * .6f; };
    player.spawn({}, ramp);
    for (int i = 0; i < 40; ++i)
        player.step({0, 1}, 0, ramp);
    require(player.grounded() && player.position().z > 2, "gentle uphill remains traversable");
    expectNear(player.position().y, ramp(0, player.position().z), "ascending follows ground");
    for (int i = 0; i < 40; ++i)
        player.step({0, -1}, 0, ramp);
    require(player.grounded(), "gentle descent remains grounded");
    expectNear(player.position().y, 0, "descending returns to flat elevation");
    const auto steep = [](float, float z) { return 1.2f * z; };
    player.spawn({}, steep);
    player.step({0, 1, true}, 0, steep);
    expectNear(player.position().z, 0, "a continuous steep grade blocks upward movement");

    const auto stairs = [](float, float z) { return z < .2f ? 0.f : z < .8f ? .16f : 1.8f; };
    player.spawn({}, stairs);
    for (int i = 0; i < 25; ++i)
        player.step({0, 1}, 0, stairs);
    require(player.grounded() && player.position().z > .7f && player.position().z < .8f,
            "step onto the low riser and stop at the wall");
    expectNear(player.position().y, .16f, "standing height on the tread");

    const auto ledge = [](float, float z) { return z < .2f ? 0.f : -2.f; };
    player.spawn({}, ledge);
    for (int i = 0; i < 5; ++i)
        player.step({0, 1}, 0, ledge);
    require(!player.grounded() && player.position().y > -.3f, "a ledge begins free fall");
    for (int i = 0; i < 40; ++i)
        player.step({0, 1}, 0, ledge);
    require(player.grounded(), "free fall ends on the lower floor");
    expectNear(player.position().y, -2, "lower floor stops vertical motion");

    const auto boundary = [](float x, float) {
        return x >= -.5f ? 0.f : std::numeric_limits<float>::quiet_NaN();
    };
    require(player.spawn({}, boundary), "spawn inside the terrain bounds");
    for (int i = 0; i < 50; ++i)
        player.step({1, 1}, 0, boundary);
    const V3 atBoundary = player.position();
    require(atBoundary.x >= -.5f && atBoundary.z > 2, "slide along an impassable boundary");
    require(!player.spawn({-1, 0, 0}, boundary), "reject spawn outside the domain");
    expectNear(length(atBoundary - player.position()), 0, "invalid spawn preserves position");
}

void solidContacts() {
    const std::vector<WalkSolid> gate{
        {{-1.27f, 1.05f, 0}, {.35f, 1.05f, .38f}},
        {{1.27f, 1.05f, 0}, {.35f, 1.05f, .38f}},
        {{0, 2.30f, 0}, {1.62f, .20f, .38f}},
    };
    PlayerController player;
    require(player.spawn({0, 1.7f, -1.5f}, flat, gate), "spawn in front of the gate");
    for (int i = 0; i < 50; ++i)
        player.step({0, 1}, 0, flat, gate);
    require(player.position().z > 1.2f && player.grounded(), "standing height fits beneath the lintel");
    for (int i = 0; i < 50; ++i)
        player.step({0, -1}, 0, flat, gate);
    expectNear(player.position().z, -1.5f, "the doorway is traversable in both directions");

    require(player.spawn({1.27f, 1.7f, -1.5f}, flat, gate), "spawn in front of a pillar");
    for (int i = 0; i < 50; ++i)
        player.step({0, 1, true}, 0, flat, gate);
    require(player.position().z <= -.64f && player.position().z >= -.80f,
            "the same scene blocks the body beside the doorway");
    require(!player.spawn({1.27f, 1.7f, 0}, flat, gate), "spawn inside a pillar is rejected");
    require(player.position().z <= -.64f, "a blocked spawn keeps the previous feet");

    require(player.spawn({0, 1.7f, 0}, flat, gate), "spawn beneath the lintel");
    float peak = 0;
    for (int i = 0; i < 60; ++i) {
        player.step({0, 0, false, i == 0}, 0, flat, gate);
        peak = std::max(peak, player.position().y);
    }
    expectNear(peak, .2f, "the head contacts the underside rather than jumping through it");
    require(player.grounded(), "the player falls back to the lower floor after an upward collision");

    const std::vector<WalkSolid> step{{{0, .08f, 0}, {.8f, .08f, .55f}}};
    require(player.spawn({0, 1.7f, -1.4f}, flat, step), "spawn at a low step");
    for (int i = 0; i < 19; ++i)
        player.step({0, 1}, 0, flat, step);
    require(player.grounded() && player.position().z > -.55f, "the low solid has a traversable tread");
    expectNear(player.position().y, .16f, "standing on top of the box");

    const std::vector<WalkSolid> platform{{{0, .57f, 0}, {.8f, .10f, .55f}}};
    require(player.spawn({0, 1.7f, -1.5f}, flat, platform), "spawn clear of a raised platform");
    for (int i = 0; i < 56; ++i)
        player.step({0, i < 25 ? 1.f : 0.f, false, i == 0}, 0, flat, platform);
    require(player.grounded(), "a descending jump can land on the raised surface");
    expectNear(player.position().y, .67f, "platform support is selected at the player's height");

    const auto slope = [](float, float z) { return -.5f * z; };
    const std::vector<WalkSolid> slopedGate{{{0, 1.0375f, 2.4f}, {1.f, .1f, .2f}}};
    require(player.spawn({}, slope, slopedGate), "spawn on a walkable slope");
    for (int i = 0; i < 56; ++i)
        player.step({0, 1}, 0, slope, slopedGate);
    expectNear(player.position().z, 3.36f, "descent under a lintel follows the slope during the sweep");
}
} // namespace

int main() {
    try {
        movement();
        jumping();
        terrain();
        solidContacts();
        std::cout << "PASS: fixed-step movement, terrain, pillars, head clearance and elevated platforms\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
