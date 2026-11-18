#pragma once
#include "vegetation.hpp"
namespace pl {
inline float shoreHeight(float x, float z) {
    return .08f * x + .12f * std::sin(z * .15f) + .02f * std::sin(x * .8f + z * .4f);
}
inline V3 shoreNormal(float x, float z) {
    return unit({-(.08f + .016f * std::cos(x * .8f + z * .4f)), 1,
                 -(.018f * std::cos(z * .15f) + .008f * std::cos(x * .8f + z * .4f))});
}
inline float shoreWaterHeight(float x, float z, float time) {
    return .025f * std::sin(x * .7f + z * .35f + time * 1.1f) +
           .012f * std::sin(z * 1.1f - x * .2f - time * .8f);
}
inline float shoreWetness(float elevation) {
    float t = std::clamp((elevation - .025f) / (.24f - .025f), 0.f, 1.f);
    return 1 - t * t * (3 - 2 * t);
}
inline Scene makeShoreScene() {
    Scene s(0, false);
    s.visualReference = true;
    s.materialShowcase = true;
    s.add(MeshType::ShoreGround, {0, 0, 0}, {1, 1, 1}, {.2f, .17f, .1f}, .9f, 0, 4);
    s.objects.back().half = {24, 3, 24};
    for (int i = 0; i < 7; ++i) {
        float x = 1.3f + float(i % 3) * 2.3f, z = -6 + float(i) * 2;
        s.add(MeshType::Rock, {x, shoreHeight(x, z) + .3f, z}, {1.4f + float(i % 2) * .5f, 1.2f, 1.6f},
              {.36f, .34f, .28f}, .8f, 0, 9);
    }
    for (int i = 0; i < 4; ++i) {
        float x = 5 + float(i % 2) * 4, z = -6 - float(i) * 2.3f, y = shoreHeight(x, z);
        appendTree(s.objects, i % 2 == 0 ? TreeSpecies::Spruce : TreeSpecies::ScotsPine,
                   {x, y, z}, 6.6f + .45f * i, 401u + 73u * uint32_t(i), shoreHeight);
    }
    const GroundPlant plants[]{GroundPlant::Juniper, GroundPlant::Fern, GroundPlant::Bellflower};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float x = 3.2f + i * 1.3f + j * .62f, z = -1.1f - j * 1.85f + i * .5f;
            appendGroundPlant(s.objects, plants[i], {x, shoreHeight(x, z), z}, 1.06f - i * .22f,
                              603u + 113u * uint32_t(i * 3 + j), shoreHeight);
        }
    }
    // Raised waterside structure: visible geometry for screen-space reflection checks.
    s.add(MeshType::Cube, {-6, 1.1f, -5}, {2.5f, 2.8f, 2.5f}, {.45f, .18f, .065f}, .65f, 0, 0);
    // Rectangular quay: deliberately planar faces to inspect material continuity across triangles.
    s.add(MeshType::Cube, {-.8f, .5f, -5}, {2.0f, 1, 5}, {.35f, .32f, .28f}, .85f, 0, 9);
    return s;
}
} // namespace pl
