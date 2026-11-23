#pragma once
#include "scene.hpp"
#include <limits>

namespace pl {
// The sun basis is fixed in world space. Moving the camera only moves the
// window across its texel grid, so sub-texel motion cannot jitter a shadow.
inline M4 shadowLightView() {
    return lookAt({-60, 100, 60}, {});
}

inline M4 snappedShadowVP(const M4 &light, V4 focus, float halfWidth, float halfHeight,
                          float nearDepth, float farDepth, int resolution) {
    const float texelX = 2 * halfWidth / std::max(1, resolution);
    const float texelY = 2 * halfHeight / std::max(1, resolution);
    const float x = std::round(focus.x / texelX) * texelX;
    const float y = std::round(focus.y / texelY) * texelY;
    return ortho(x - halfWidth, x + halfWidth, y - halfHeight, y + halfHeight,
                 nearDepth, farDepth) * light;
}

inline M4 firstPersonShadowVP(const Camera &camera, int resolution) {
    const M4 light = shadowLightView();
    const V4 focus = light * V4{camera.pos.x, 0, camera.pos.z, 1};
    const float offset = std::round((light.at(2, 3) - focus.z) / 4.f) * 4.f;
    return snappedShadowVP(light, focus, 85, 85, offset + .1f, offset + 250, resolution);
}

inline M4 strategyShadowVP(const Scene &scene, const Camera &camera, float aspect, int resolution) {
    const M4 light = shadowLightView();
    const V3 at = camera.target;
    const V4 focus = light * V4{at.x, at.y, at.z, 1};
    // Keep the projection scale fixed within each zoom step. A chunk entering
    // the camera frustum must not rescale every shadow already on screen.
    const float halfWidth = std::ceil((camera.distance * std::max(1.20f, aspect * .82f) + 7) / 2) * 2;
    const float halfHeight = std::ceil((camera.distance * 1.04f + 8) / 2) * 2;
    float nearDepth = std::numeric_limits<float>::infinity();
    float farDepth = -nearDepth;
    auto includeDepth = [&](V3 center, V3 half) {
        const float depth = -(light.at(2, 0) * center.x + light.at(2, 1) * center.y +
                              light.at(2, 2) * center.z + light.at(2, 3));
        const float radius = std::abs(light.at(2, 0)) * half.x + std::abs(light.at(2, 1)) * half.y +
                             std::abs(light.at(2, 2)) * half.z;
        nearDepth = std::min(nearDepth, depth - radius);
        farDepth = std::max(farDepth, depth + radius);
    };
    size_t covered = 0;
    for (const auto &region : scene.regions) {
        for (; covered < region.begin; ++covered)
            includeDepth(scene.objects[covered].center, scene.objects[covered].half);
        includeDepth(region.center, region.half);
        covered = region.end;
    }
    // Off-screen resident regions still cast shadows. Also include the actors
    // and any other objects outside the ranges covered by the resident bounds.
    for (; covered < scene.objects.size(); ++covered)
        includeDepth(scene.objects[covered].center, scene.objects[covered].half);
    if (farDepth < nearDepth) {
        nearDepth = -focus.z - 125;
        farDepth = -focus.z + 125;
    }
    return snappedShadowVP(light, focus, halfWidth, halfHeight,
                           std::floor(nearDepth / 4.f) * 4.f - 4,
                           std::ceil(farDepth / 4.f) * 4.f + 4, resolution);
}
} // namespace pl
