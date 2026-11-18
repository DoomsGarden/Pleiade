#pragma once
#include "scene.hpp"
#include <limits>
namespace pl {
// Fit loaded receivers in visible regions, including whole conservative bounds.
// Off-screen loaded casters still contribute depth and are rendered by the light frustum.
inline M4 strategyShadowVP(const Scene &scene, const Camera &camera, float aspect, int resolution) {
    M4 light = lookAt(V3{-60, 100, 60}, V3{});
    Frustum view(camera.projection(aspect) * camera.view());
    float loX = 1e30f, hiX = -1e30f, loY = 1e30f, hiY = -1e30f;
    float loZ = 1e30f, hiZ = -1e30f;
    bool receivers = false;
    auto include = [&](V3 c, V3 h) {
        bool visible = view.visible(c, h);
        receivers |= visible;
        auto p = light * V4{c.x, c.y, c.z, 1};
        V3 e{};
        e.x = std::abs(light.at(0, 0)) * h.x + std::abs(light.at(0, 1)) * h.y +
              std::abs(light.at(0, 2)) * h.z;
        e.y = std::abs(light.at(1, 0)) * h.x + std::abs(light.at(1, 1)) * h.y +
              std::abs(light.at(1, 2)) * h.z;
        e.z = std::abs(light.at(2, 0)) * h.x + std::abs(light.at(2, 1)) * h.y +
              std::abs(light.at(2, 2)) * h.z;
        loZ = std::min(loZ, p.z - e.z);
        hiZ = std::max(hiZ, p.z + e.z);
        if (visible) {
            loX = std::min(loX, p.x - e.x);
            hiX = std::max(hiX, p.x + e.x);
            loY = std::min(loY, p.y - e.y);
            hiY = std::max(hiY, p.y + e.y);
        }
    };
    size_t covered = 0;
    for (const auto &r : scene.regions) {
        // Regions cover a prefix; moving actors are appended outside them.
        for (; covered < r.begin; ++covered)
            include(scene.objects[covered].center, scene.objects[covered].half);
        include(r.center, r.half);
        covered = r.end;
    }
    for (; covered < scene.objects.size(); ++covered)
        include(scene.objects[covered].center, scene.objects[covered].half);
    if (!receivers) {
        V3 c = camera.target;
        c.y = 0;
        return ortho(-85, 85, -85, 85, .1f, 250) * lookAt(c + V3{-60, 100, 60}, c);
    }
    // Reserve PCF/bounds margin, quantize size, and snap center to shadow texels.
    float extentX = std::ceil((hiX - loX + 4) / 4) * 4;
    float extentY = std::ceil((hiY - loY + 4) / 4) * 4;
    float texelX = extentX / resolution, texelY = extentY / resolution;
    float cx = std::round((loX + hiX) * .5f / texelX) * texelX;
    float cy = std::round((loY + hiY) * .5f / texelY) * texelY;
    return ortho(cx - extentX * .5f, cx + extentX * .5f, cy - extentY * .5f, cy + extentY * .5f,
                 -hiZ - 4, -loZ + 4) *
           light;
}
} // namespace pl
