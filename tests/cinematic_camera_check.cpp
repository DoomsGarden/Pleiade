#include "../tools/desktop-source/cinematic_camera.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace pl;

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool same(V3 a, V3 b, float tolerance = 1e-4f) {
    return length(a - b) <= tolerance;
}

template <typename Action> void rejects(Action action, const char *message) {
    try {
        action();
    } catch (const std::invalid_argument &) {
        return;
    }
    throw std::runtime_error(message);
}
} // namespace

int main() {
    using namespace pl;
    const CinematicKey first{0, {0, 3, 10}, {0, 1, 0}, 45};
    const CinematicKey middle{2, {2, 4, 8}, {0, 1, 0}, 50};
    const CinematicKey last{5, {6, 3, 6}, {1, 1, 0}, 61};
    const CinematicKey cut{6, {-6, 2, 1}, {1, 2, 0}, 35, true};
    const CinematicKey end{8, {-4, 2, 1}, {1, 2, 0}};
    const CinematicTrack track({first, middle, last, cut, end});

    require(track.duration() == 8, "Duration differs from the last key");
    require(same(track.sample(-1).position, first.position), "Pre-roll does not hold the first pose");
    require(same(track.sample(20).position, end.position), "Post-roll does not hold the last pose");
    require(same(track.sample(2).position, middle.position), "The camera does not reach its key position");
    require(track.sample(2).verticalFovDegrees == 50, "The camera does not reach its key field of view");
    require(same(track.sample(5.999).position, last.position), "An impending cut changed the preceding shot");
    require(same(track.sample(6).position, cut.position), "Cut failed at its exact timestamp");
    require(same(track.sample(6).target, cut.target), "Cut did not switch the target");
    require(track.sample(6).verticalFovDegrees == 35, "Cut did not switch the field of view");
    require(same(track.sample(7).position, {-5, 2, 1}), "The camera did not resume smoothly after a cut");
    require(same(track.sample(1).target, first.target), "A stationary target should remain stationary");

    constexpr double dt = .001;
    auto left = track.sample(2 - dt), right = track.sample(2 + dt);
    V3 incoming = (middle.position - left.position) * float(1 / dt);
    V3 outgoing = (right.position - middle.position) * float(1 / dt);
    require(same(incoming, outgoing, .012f) && incoming.x > .5f, "Camera velocity jumps at a smooth key");
    require(std::abs((50.f - left.verticalFovDegrees) - (right.verticalFovDegrees - 50.f)) < .0001f,
            "Lens speed jumps at a smooth key");
    require(std::abs(track.sample(6 + dt).position.x - cut.position.x) < 1e-5f,
            "A previous shot pulls on the path after a cut");

    for (int i = 1; i < 100; ++i) {
        auto pose = track.sample(2 + 3 * i / 100.0);
        require(pose.position.x >= 2 && pose.position.x <= 6 && pose.position.y >= 3 && pose.position.y <= 4,
                "Camera path overshot an endpoint");
        require(pose.verticalFovDegrees >= 50 && pose.verticalFovDegrees <= 61, "Lens overshot an endpoint");
        require(pose.target.x >= 0 && pose.target.x <= 1, "Look-at path overshot its endpoint");
    }
    auto initial = track.sample(3.25);
    track.sample(8);
    track.sample(0);
    auto seek = track.sample(3.25);
    require(same(initial.position, seek.position, 0) && same(initial.target, seek.target, 0) &&
                initial.verticalFovDegrees == seek.verticalFovDegrees,
            "Track sampling depends on the playback history");
    Camera camera;
    camera.pos = seek.position;
    camera.face(seek.target);
    require(same(camera.forward(), unit(seek.target - seek.position)), "The pose is not usable by Camera");

    camera.pos = {5, 10, 10};
    camera.face({5, 0, 10});
    auto overhead = camera.view();
    auto center = overhead * V4{5, 0, 10, 1};
    require(same({center.x, center.y, center.z}, {0, 0, -10}),
            "A straight-down camera does not center its focus");
    const V3 screenRight{overhead.at(0, 0), overhead.at(0, 1), overhead.at(0, 2)};
    const V3 screenUp{overhead.at(1, 0), overhead.at(1, 1), overhead.at(1, 2)};
    require(length(cross(screenRight, screenUp)) > .999f,
            "A straight-down camera cannot project screen axes");

    const CinematicTrack throughFocus({{0, {-1, 0, 0}, {0, 0, 0}}, {2, {1, 0, 0}, {0, 0, 0}}});
    auto crossing = throughFocus.sample(1);
    require(length(crossing.target - crossing.position) > .5f, "Focus crossing collapsed the look direction");

    rejects([] { CinematicTrack({}); }, "Empty track was accepted");
    rejects([&] { CinematicTrack({first, first}); }, "Duplicate key timestamp was accepted");
    rejects([&] { CinematicTrack({middle, first}); }, "Decreasing key time was accepted");
    rejects([&] { CinematicTrack({first, {1, {1, 1, 1}, {1, 1, 1}}}); }, "Zero look direction was accepted");
    rejects([&] { CinematicTrack({{0, first.position, first.target, 180}}); }, "Invalid lens was accepted");
    rejects([&] { track.sample(std::numeric_limits<double>::quiet_NaN()); }, "Non-finite sample was accepted");
    std::cout << "Cinematic camera interpolation, cuts and deterministic seeking passed\n";
}
