#include "../tools/desktop-source/shadow_fit.hpp"
#include <iostream>
#include <stdexcept>

using namespace pl;

namespace {
constexpr int resolution = 2048;
constexpr float aspect = 16.f / 9.f;

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

void checkPan(const Scene &scene, Camera camera) {
    auto shadow = [&] {
        return camera.strategy ? strategyShadowVP(scene, camera, aspect, resolution)
                               : firstPersonShadowVP(camera, resolution);
    };
    const V4 receiver{.75f, 0, 1.5f, 1};
    const V4 start = shadow() * receiver;
    V3 &focus = camera.strategy ? camera.target : camera.pos;
    bool shifted = false;
    int unchanged = 0;
    for (int i = 1; i <= 100; ++i) {
        focus.x += .002f;
        const V4 projected = shadow() * receiver;
        const float dx = (projected.x - start.x) * (.5f * resolution);
        const float dy = (projected.y - start.y) * (.5f * resolution);
        require(std::abs(dx - std::round(dx)) < .002f &&
                    std::abs(dy - std::round(dy)) < .002f,
                "A stationary receiver moved by a fraction of a shadow texel");
        shifted |= std::abs(dx) + std::abs(dy) > .5f;
        unchanged += std::abs(dx) + std::abs(dy) < .002f;
    }
    require(shifted && unchanged > 0, "The shadow map must track the camera in whole texel steps");
}
} // namespace

int main() {
    try {
        Scene scene{0, false};
        scene.add(MeshType::Terrain, {0, -1, 0}, {80, 2, 80}, {.5f, .5f, .5f});
        // Sunward, shares the receiver's shadow-map XY and sits outside the camera view.
        const V3 elevatedCaster{-90, 150, 90};
        scene.add(MeshType::Cube, elevatedCaster, {4, 4, 4}, {.5f, .5f, .5f});
        scene.regions.push_back({0, 1, {0, -1, 0}, {40, 1, 40}});
        Camera camera;
        camera.pos = {0, 2, 0};
        checkPan(scene, camera);
        camera.strategy = true;
        checkPan(scene, camera);

        const M4 before = strategyShadowVP(scene, camera, aspect, resolution);
        camera.orbit = 1.1f;
        const M4 rotated = strategyShadowVP(scene, camera, aspect, resolution);
        require(before.v == rotated.v, "Orbiting at a fixed zoom changed the shadow projection");
        const Frustum view(camera.projection(aspect) * camera.view());
        require(!view.visible(elevatedCaster, {2, 2, 2}), "Off-screen caster fixture is in view");
        const V4 projectedCaster = rotated * V4{elevatedCaster.x, elevatedCaster.y, elevatedCaster.z, 1};
        require(std::abs(projectedCaster.x) < .01f && std::abs(projectedCaster.y) < .01f &&
                    std::abs(projectedCaster.z) < 1.f,
                "Off-screen caster lies outside the light volume");

        camera.distance = 18;
        const M4 zoomed = strategyShadowVP(scene, camera, aspect, resolution);
        require(std::abs(zoomed.at(0, 0)) > std::abs(rotated.at(0, 0)),
                "Zooming in did not increase local shadow resolution");
        const M4 empty = strategyShadowVP(Scene{0, false}, camera, aspect, resolution);
        require(std::all_of(empty.v.begin(), empty.v.end(), [](float v) { return std::isfinite(v); }),
                "Empty world has an invalid shadow projection");
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
    std::cout << "PASS: stable first-person and strategy shadows, off-screen casters and zoom\n";
}
