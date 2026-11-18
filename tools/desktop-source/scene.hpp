#pragma once
#include "math.hpp"
#include <cstdint>

namespace pl {
enum class MeshType {
    Cube,
    Sphere,
    Foliage,
    FoliageFar,
    Grass,
    Terrain,
    Roof,
    Leaves,
    LeavesFar,
    Branch,
    Meadow,
    Rock,
    TerrainCoarse,
    LeavesDetail,
    CanopyImpostor,
    ShoreGround,
    RockDetail,
    TreeWood,
    SpruceNeedles,
    PineNeedles,
    JuniperNeedles,
    FernFrond,
    BellflowerStem,
    BellflowerBloom,
    SpruceNeedlesFar,
    PineNeedlesFar,
    JuniperNeedlesFar,
    FernFrondFar,
    TreeTrunk,
    BirchLeaves,
    BirchLeavesFar,
    BambooStem,
    BambooLeaves,
    PalmFrond,
    BeveledBox,
    TreeTrunkFar,
    BambooStemFar,
    BambooLeavesFar,
    PalmFrondFar,
    Count
};
constexpr int meshCount = static_cast<int>(MeshType::Count);
enum class SurfaceLayer {
    PineNeedle, ConiferNeedle, FernPinna, Broadleaf, Petal, Bark, PineBark, Timber,
    Masonry, Sand, Snow, Stone, Soil, Bamboo, PineSpray, ConiferSpray, Count
};
inline constexpr float materialPattern(SurfaceLayer layer) { return 20.f + float(layer); }
inline constexpr float terrainPattern(SurfaceLayer layer) { return 40.f + float(layer); }
struct Vertex {
    V3 p, n;
    float u = 0, v = 0;
};
enum class VegetationSection : uint8_t {
    None, Collar, Root, Trunk, PrimaryBranch, SecondaryBranch, Shoot, Foliage, Flower
};
struct Instance {
    M4 model;
    V4 color;
    V4 surface;
}; // roughness, metallic, pattern, deformation (0 none, 1 terrain, 2 grass)
struct Object {
    V3 center, half;
    MeshType mesh;
    Instance gpu;
    int lodClass = 0; // 1 foliage, 2 grass, 3 fine vegetation
    VegetationSection vegetation = VegetationSection::None;
    uint16_t parentDistance = 0; // Backward offset in the contiguous plant objects, zero at the collar.
    bool visible = true; // Logical sections may share a continuous rendered surface.
};
struct ObjectRegion {
    size_t begin = 0, end = 0;
    V3 center{}, half{};
};
inline ObjectRegion objectRegion(const std::vector<Object> &objects, size_t begin, size_t end) {
    if (begin >= end || end > objects.size())
        throw std::invalid_argument("Invalid region range");
    V3 lo = objects[begin].center - objects[begin].half,
       hi = objects[begin].center + objects[begin].half;
    for (size_t i = begin + 1; i < end; ++i) {
        const auto &o = objects[i];
        V3 a = o.center - o.half, b = o.center + o.half;
        lo = {std::min(lo.x, a.x), std::min(lo.y, a.y), std::min(lo.z, a.z)};
        hi = {std::max(hi.x, b.x), std::max(hi.y, b.y), std::max(hi.z, b.z)};
    }
    return {begin, end, (lo + hi) * .5f, (hi - lo) * .5f + V3{.001f, .001f, .001f}};
}
struct Scene {
    std::vector<Object> objects;
    std::vector<ObjectRegion> regions;
    bool dynamicWorld = false;
    bool visualReference = false;
    bool materialShowcase = false;
    uint64_t revision = 0;
    double time = 0;
    void add(MeshType mesh, V3 p, V3 size, V3 color, float rough = .7f, float metal = 0,
             float pattern = 1) {
        if (size.x <= 0 || size.y <= 0 || size.z <= 0)
            throw std::invalid_argument("Non-positive scale");
        objects.push_back(
            {p,
             size * .5f,
             mesh,
             {transform(p, size), {color.x, color.y, color.z, 1}, {rough, metal, pattern, 0}}});
    }
    explicit Scene(int stress = 0, bool courtyard = true) {
        if (!courtyard)
            return;
        auto cube = [&](V3 p, V3 s, V3 c, float r = .75f, float m = 0, float pattern = 1) {
            add(MeshType::Cube, p, s, c, r, m, pattern);
        };
        V3 stone{.57f, .51f, .40f}, light{.72f, .67f, .56f}, dark{.075f, .095f, .105f},
            bronze{.65f, .32f, .105f};
        cube({0, -.65f, 0}, {180, 1, 180}, {.17f, .20f, .17f}, .94f);
        cube({0, -.24f, 0}, {37, .48f, 33}, stone);
        cube({0, .04f, 0}, {33, .08f, 29}, light, .72f, 0, 2);
        // Four steps on the entrance side.
        for (int i = 0; i < 4; ++i)
            cube({0, -.33f + i * .10f, 17.f - i * .8f}, {12, .2f, 1.6f}, light);
        // Side colonnades; bases, shafts and capitals share one instanced cube mesh.
        for (int side : {-1, 1}) {
            for (int j = 0; j < 7; ++j) {
                float x = side * 13.f, z = -12 + j * 4.f;
                cube({x, .32f, z}, {1.65f, .50f, 1.65f}, light);
                cube({x, 3.1f, z}, {.88f, 5.1f, .88f}, stone);
                cube({x, 5.85f, z}, {1.5f, .4f, 1.5f}, light);
            }
            cube({side * 13.f, 6.25f, 0}, {1.8f, .45f, 27}, light);
            cube({side * 15.4f, 2.1f, 0}, {.55f, 4.1f, 28}, stone);
            // Narrow decorative reflecting material. No planar reflections or water simulation.
            cube({side * 7.2f, .15f, 1}, {4.7f, .18f, 19}, dark);
            cube({side * 7.2f, .255f, 1}, {4.35f, .03f, 18.65f}, {.025f, .14f, .15f}, .18f, .65f, 0);
            for (int j = 0; j < 5; ++j) {
                float z = -7 + j * 4.f;
                cube({side * 10.1f, .42f, z}, {.9f, .7f, .9f}, dark);
                add(MeshType::Sphere, {side * 10.1f, 1.1f, z}, {1.25f, 1.1f, 1.25f}, {.10f, .19f, .075f},
                    .9f, 0, 1);
            }
        }
        // Rear wall with alcoves and a central opening.
        for (int side : {-1, 1}) {
            cube({side * 9.1f, 4.1f, -13.5f}, {10.2f, 8.2f, .8f}, stone);
            cube({side * 4.2f, 4.25f, -13.f}, {1.1f, 8.5f, 1.4f}, light);
            for (int i = 0; i < 4; ++i)
                cube({side * (6.0f + i * 2.1f), 4, -12.99f}, {1.25f, 5.4f, .12f}, dark);
        }
        cube({0, 8.45f, -13.5f}, {29, .65f, 1.8f}, light);
        cube({0, 7.35f, -13.5f}, {7.3f, 1.5f, .8f}, stone);
        cube({0, .25f, -1}, {5, .35f, 5}, dark);
        cube({0, .65f, -1}, {3.8f, .45f, 3.8f}, light);
        cube({0, 1.25f, -1}, {2.7f, .8f, 2.7f}, stone);
        add(MeshType::Sphere, {0, 3.45f, -1}, {3.5f, 3.5f, 3.5f}, bronze, .23f, .92f, 0);
        // Seven satellites: the Pleiades motif, physically lit test materials.
        for (int i = 0; i < 7; ++i) {
            float a = i * 2 * pi / 7;
            add(MeshType::Sphere,
                {std::cos(a) * 3.1f, 4.5f + std::sin(a) * 1.2f, -1 + std::sin(a) * 2.6f},
                {.42f, .42f, .42f}, light, .28f, .35f, 0);
        }
        // Deterministic stress field outside the courtyard. Not game units yet.
        uint32_t seed = 0x504c4549;
        for (int i = 0; i < stress; ++i) {
            seed = 1664525u * seed + 1013904223u;
            float h = .6f + (seed & 255) / 255.f * 3;
            int col = i % 100, row = i / 100;
            float x = (col - 50) * 1.1f, z = -24 - row * 1.1f;
            cube({x, h * .5f - .1f, z}, {.65f, h, .65f}, {.28f, .33f, .32f});
        }
    }
};
} // namespace pl
