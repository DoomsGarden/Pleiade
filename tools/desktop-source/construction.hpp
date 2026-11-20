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
    constexpr float windowX = 1.98f, windowHalf = .526f, windowBottom = .865f, windowTop = 1.835f;
    constexpr float doorHalf = .572f, doorTop = 2.056f;
    struct Opening { float left, right, bottom, top; };
    const std::array openings{
        Opening{-windowX-windowHalf,-windowX+windowHalf,windowBottom,windowTop},
        Opening{-doorHalf,doorHalf,0.f,doorTop},
        Opening{windowX-windowHalf,windowX+windowHalf,windowBottom,windowTop}};
    for (int row = 0; row < 8; ++row) {
        const float bottom = row*courses, top = (row+1)*courses;
        const float y = floor + (bottom + top)*.5f;
        const V3 wood = timber * (.92f + .10f * std::sin(float(row) * 2.43f));
        part({0, y, back}, {6.76f, .305f, .275f}, wood, SurfaceLayer::Timber);
        for (float side : {-1.f, 1.f})
            part({side * width, y, 0}, {.28f, .304f, depth * 2}, wood, SurfaceLayer::Timber);
        auto face=[&](float left,float right,float lo,float hi){
            if(right-left>.01f&&hi-lo>.006f)
                part({(left+right)*.5f,floor+(lo+hi)*.5f,front},{right-left,hi-lo,.28f},wood,SurfaceLayer::Timber);
        };
        float left=-3.44f;
        for(const auto& cut:openings){
            face(left,cut.left,bottom,top);
            face(cut.left,cut.right,bottom,std::min(top,cut.bottom));
            face(cut.left,cut.right,std::max(bottom,cut.top),top);
            left=cut.right;
        }
        face(left,3.44f,bottom,top);
    }
    const float run = 3.91f, rise = 1.65f;
    const float roofAngle = std::atan2(rise, run);
    const float roofEave = floor + wall - .33f, ridge = roofEave + rise;
    const float gableHalf = width + .12f;
    const float gableBase = floor + courses * 8 - .034f;
    // The gable meets the underside of the deck on both slopes.
    scene.add(MeshType::Roof, {0, (gableBase + ridge) * .5f, 0},
              {gableHalf * 2, ridge - gableBase, depth * 2 + .24f}, timber,
              .82f, 0, materialPattern(SurfaceLayer::Timber));
    // A full deck spans the overhang. The face boards sit on this structure.
    for (float side : {-1.f, 1.f})
        part({side * run * .5f, roofEave + rise * .5f, 0},
             {std::hypot(run, rise) + .035f, .17f, 6.40f}, beam, SurfaceLayer::Timber, -side * roofAngle);
    for (float side : {-1.f, 1.f}) {
        for (float z : {-3.19f, 3.19f})
            part({side * run * .5f, roofEave + rise * .5f + .035f, z},
                 {std::hypot(run, rise) + .04f, .23f, .14f}, timber * .82f, SurfaceLayer::Timber, -side * roofAngle);
        for (int rafter = -2; rafter <= 2; ++rafter)
            part({side * 3.56f, ridge - 3.56f * rise / run - .13f, rafter * 1.13f},
                 {.71f, .19f, .16f}, beam, SurfaceLayer::Timber, -side * roofAngle);
    }
    for (int board = 0; board < 13; ++board) {
        float z = -2.94f + board * .49f;
        for (float side : {-1.f, 1.f}) {
            V3 tint = V3{.58f, .53f, .46f} * (.92f + .09f * std::sin(board * 3.f));
            part({side * run * .5f, roofEave + rise * .5f + .112f, z},
                 {std::hypot(run, rise), .104f, .51f}, snowy ? V3{1.f, 1.f, 1.f} : tint,
                 snowy ? SurfaceLayer::Snow : SurfaceLayer::Timber, -side * roofAngle);
        }
    }
    part({0, ridge + .13f, 0}, {.26f, .16f, 6.43f}, beam, SurfaceLayer::Timber);
    part({1.83f, ridge - .27f, -1.22f}, {.66f, 1.82f, .71f}, stone, SurfaceLayer::Masonry);
    part({1.83f, ridge + .68f, -1.22f}, {.82f, .17f, .81f}, stone, SurfaceLayer::Masonry);

    for (float side : {-1.f, 1.f}) {
        const float x = side * windowX, y = floor + (windowBottom+windowTop)*.5f, z = front + .065f;
        for(float dx : {-.259f,.259f})
            for(float dy : {-.232f,.232f})
                scene.add(MeshType::Cube,{x+dx,y+dy,z+.054f},{.488f,.431f,.013f},
                          {.016f,.026f,.024f},.085f,0,18.f);
        for (float dx : {-.55f, 0.f, .55f})
            part({x + dx, y, z + .12f}, {.105f, 1.12f, .15f}, beam, SurfaceLayer::Timber);
        for (float dy : {-.50f, 0.f, .50f})
            part({x, y + dy, z + .12f}, {1.22f, .095f, .15f}, beam, SurfaceLayer::Timber);
        part({x, y - .61f, z + .20f}, {1.37f, .12f, .33f}, timber, SurfaceLayer::Timber);
    }
    part({0,floor-.006f,0},{6.6f,.098f,4.76f},beam*.61f,SurfaceLayer::Timber);
    for(float side : {-1.f,1.f}) {
        part({side*2.45f,floor+.74f,-1.85f},{1.20f,.11f,.51f},timber*.60f,SurfaceLayer::Timber);
        for(float offset : {-.43f,.43f})
            part({side*2.45f+offset,floor+.37f,-1.85f},{.10f,.74f,.45f},beam*.49f,SurfaceLayer::Timber);
    }
    part({0, floor + 1.031f, front + .012f}, {1.13f, 2.08f, .15f}, beam * .71f, SurfaceLayer::Timber);
    for (int board = -2; board <= 2; ++board) {
        part({board * .218f, floor + 1.035f, front + .10f}, {.216f, 2.045f, .094f},
             timber * (.69f + .037f * std::sin(board * 4.2f)), SurfaceLayer::Timber);
        scene.objects.back().gpu.surface.w = -5;
    }
    for (float side : {-1.f, 1.f})
        part({side * .62f, floor + 1.034f, front + .112f}, {.144f, 2.18f, .165f}, beam, SurfaceLayer::Timber);
    part({0, floor + 2.11f, front + .115f}, {1.39f, .15f, .165f}, beam, SurfaceLayer::Timber);
    part({.36f, floor + 1.04f, front + .162f}, {.14f, .075f, .12f}, stone, SurfaceLayer::Stone);
    for (int row = 0; row < 3; ++row) {
        float z = front + .36f + row * .46f;
        float height = floor - .19f * row;
        float low = shoreHeight(0, z) - .08f;
        part({0, (height + low) * .5f, z}, {2.18f, height - low, .55f}, stone, SurfaceLayer::Masonry);
    }
}

enum class BuildingForm { Cabin, Stone, Shelter };

inline Scene makeBuildingScene(BuildingForm form=BuildingForm::Cabin) {
    Scene scene(0, false);
    scene.add(MeshType::ShoreGround, {0, 0, 0}, {1, 1, 1}, {.20f, .17f, .10f}, .93f, 0, 4);
    scene.objects.back().half = {24, 3, 24};
    if(form==BuildingForm::Cabin)appendCabin(scene);
    else {
        const bool stone=form==BuildingForm::Stone;
        auto piece=[&](V3 at,V3 dimensions,V3 color,SurfaceLayer layer){
            scene.add(MeshType::BeveledBox,at,dimensions,color,.83f,0,materialPattern(layer));
        };
        const V3 timber{.81f,.80f,.72f}, masonry{.91f,.90f,.87f};
        piece({0,.10f,0},{5.6f,.45f,4.2f},masonry,SurfaceLayer::Masonry);
        if(stone){
            for(float side : {-1.f,1.f}){
                piece({side*2.51f,1.62f,0},{.48f,2.89f,4.0f},masonry,SurfaceLayer::Masonry);
                piece({side*1.72f,1.62f,1.94f},{1.61f,2.89f,.38f},masonry,SurfaceLayer::Masonry);
            }
            piece({0,1.62f,-1.94f},{5.12f,2.89f,.46f},masonry,SurfaceLayer::Masonry);
            piece({0,2.72f,1.96f},{2.22f,.66f,.51f},masonry,SurfaceLayer::Masonry);
            piece({0,1.33f,1.78f},{1.52f,2.03f,.12f},timber*.72f,SurfaceLayer::Timber);
            piece({1.73f,3.55f,-.86f},{.64f,1.56f,.68f},masonry,SurfaceLayer::Masonry);
        }else{
            for(float x : {-2.35f,2.35f})for(float z : {-1.75f,1.75f}) {
                piece({x,.33f,z},{.43f,.50f,.43f},masonry,SurfaceLayer::Masonry);
                piece({x,1.71f,z},{.25f,2.75f,.25f},timber,SurfaceLayer::Timber);
            }
            piece({0,2.88f,1.76f},{5.16f,.24f,.25f},timber*.82f,SurfaceLayer::Timber);
            piece({0,.76f,-1.37f},{3.98f,.16f,.57f},timber,SurfaceLayer::Timber);
        }
        const float rise=stone?1.67f:1.03f;
        scene.add(MeshType::Roof,{0,2.99f+rise*.5f,0},{6.22f,rise,4.90f},
                  stone?V3{.58f,.60f,.58f}:V3{.65f,.58f,.49f},.90f,0,
                  materialPattern(stone?SurfaceLayer::Stone:SurfaceLayer::Timber));
    }
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
