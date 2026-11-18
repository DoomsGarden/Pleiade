#pragma once
#include "shore.hpp"

namespace pl {
inline void appendCabin(Scene &scene, bool snowy = false) {
    const float floor = .62f, wall = 2.55f, depth = 2.45f, width = 3.4f;
    const V3 timber{.94f, .92f, .86f}, beam{.76f, .77f, .75f};
    const V3 stone{.88f, .89f, .89f};
    auto part = [&](V3 center, V3 size, V3 tint, SurfaceLayer material, float angle = 0) {
        scene.add(MeshType::BeveledBox, center, size, tint, .84f, 0, materialPattern(material));
        if (angle == 0) return;
        auto &object = scene.objects.back();
        auto &model = object.gpu.model;
        const float c = std::cos(angle), s = std::sin(angle);
        model.at(0, 0) = size.x * c; model.at(1, 0) = size.x * s;
        model.at(0, 1) = -size.y * s; model.at(1, 1) = size.y * c;
        object.half.x = (std::abs(c) * size.x + std::abs(s) * size.y) * .5f;
        object.half.y = (std::abs(s) * size.x + std::abs(c) * size.y) * .5f;
    };
    for (float x : {-2.85f, 0.f, 2.85f})
        for (float z : {-1.91f, 1.91f}) {
            const float low = shoreHeight(x, z) - .11f;
            part({x, (floor + low) * .5f, z}, {.78f, floor - low, .74f}, stone, SurfaceLayer::Masonry);
        }
    part({0, floor - .095f, 0}, {6.95f, .23f, 5.08f}, beam, SurfaceLayer::Timber);

    const float front = depth, back = -depth;
    const float courses = .30f;
    for (int row = 0; row < 8; ++row) {
        const float y = floor + (float(row) + .5f) * courses;
        const V3 wood = timber * (.92f + .10f * std::sin(float(row) * 2.43f));
        part({0, y, back}, {6.76f, .305f, .275f}, wood, SurfaceLayer::Timber);
        for (float side : {-1.f, 1.f}) {
            part({side * width, y, 0}, {.28f, .304f, depth * 2}, wood, SurfaceLayer::Timber);
            if (row < 3 || row > 5) {
                part({side * 1.99f, y, front}, {2.74f, .305f, .28f}, wood, SurfaceLayer::Timber);
            } else {
                part({side * 1.02f, y, front}, {.80f, .305f, .28f}, wood, SurfaceLayer::Timber);
                part({side * 2.88f, y, front}, {1.01f, .305f, .28f}, wood, SurfaceLayer::Timber);
            }
        }
        if (row == 7) part({0, y, front}, {1.24f, .305f, .30f}, wood, SurfaceLayer::Timber);
    }
    const float eaves = floor + wall;
    const float rise = 1.65f;
    scene.add(MeshType::Roof, {0, eaves + rise * .5f - .17f, 0}, {6.72f, rise, 5.02f}, beam,
              .82f, 0, materialPattern(SurfaceLayer::Timber));
    const float run = 3.91f, roofAngle = std::atan2(rise, run);
    for (int board = 0; board < 13; ++board) {
        float z = -2.94f + board * .49f;
        for (float side : {-1.f, 1.f}) {
            V3 tint = V3{.58f, .53f, .46f} * (.92f + .09f * std::sin(board * 3.f));
            part({side * run * .5f, eaves + rise * .5f + .02f, z},
                 {std::hypot(run, rise), .13f, .51f}, snowy ? V3{1.f, 1.f, 1.f} : tint,
                 snowy ? SurfaceLayer::Snow : SurfaceLayer::Timber, -side * roofAngle);
        }
    }
    part({0, eaves + rise + .04f, 0}, {.23f, .14f, 6.43f}, beam, SurfaceLayer::Timber);
    part({1.83f, eaves + rise - .12f, -1.22f}, {.66f, 1.82f, .71f}, stone, SurfaceLayer::Masonry);
    part({1.83f, eaves + rise + .83f, -1.22f}, {.82f, .17f, .81f}, stone, SurfaceLayer::Masonry);

    for (float side : {-1.f, 1.f}) {
        const float x = side * 1.98f, y = floor + 1.35f, z = front + .065f;
        scene.add(MeshType::Cube, {x, y, z}, {1.03f, .91f, .08f}, {.035f, .061f, .057f}, .32f, .12f, 0);
        for (float dx : {-.55f, 0.f, .55f})
            part({x + dx, y, z + .12f}, {.105f, 1.12f, .15f}, beam, SurfaceLayer::Timber);
        for (float dy : {-.50f, 0.f, .50f})
            part({x, y + dy, z + .12f}, {1.22f, .095f, .15f}, beam, SurfaceLayer::Timber);
        part({x, y - .61f, z + .20f}, {1.37f, .12f, .33f}, timber, SurfaceLayer::Timber);
    }
    part({0, floor + 1.031f, front + .049f}, {1.13f, 2.08f, .16f}, beam, SurfaceLayer::Timber);
    part({.36f, floor + 1.04f, front + .162f}, {.14f, .075f, .12f}, stone, SurfaceLayer::Stone);
    for (int row = 0; row < 3; ++row) {
        float z = front + .36f + row * .46f;
        float height = floor - .19f * row;
        float low = shoreHeight(0, z) - .08f;
        part({0, (height + low) * .5f, z}, {2.18f, height - low, .55f}, stone, SurfaceLayer::Masonry);
    }
}

inline Scene makeBuildingScene() {
    Scene scene(0, false);
    scene.add(MeshType::ShoreGround, {0, 0, 0}, {1, 1, 1}, {.20f, .17f, .10f}, .93f, 0, 4);
    scene.objects.back().half = {24, 3, 24};
    appendCabin(scene);
    VegetationRandom random(4770);
    for (int i = 0; i < 13; ++i) {
        const float x = random.range(-8, 8), z = random.range(3.5f, 8.6f);
        const float size = random.range(.20f, .67f);
        scene.add(MeshType::RockDetail, {x, shoreHeight(x, z) + size * .13f, z},
                  {size, size * random.range(.46f, .79f), size * random.range(.8f, 1.30f)},
                  V3{.94f, .89f, .81f} * random.range(.82f, 1.12f), .85f, 0, materialPattern(SurfaceLayer::Stone));
    }
    return scene;
}
} // namespace pl
