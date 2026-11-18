#pragma once
#include "vegetation.hpp"
#include <chrono>
#include <map>
#include <utility>

namespace pl {
inline float terrainHeight(float x, float z) {
    return 2.0f * std::sin(x * .04f) * std::cos(z * .045f) + .65f * std::sin(x * .095f + z * .075f);
}
inline V3 terrainNormal(float x, float z) {
    float dx =
        .08f * std::cos(x * .04f) * std::cos(z * .045f) + .06175f * std::cos(x * .095f + z * .075f);
    float dz =
        -.09f * std::sin(x * .04f) * std::sin(z * .045f) + .04875f * std::cos(x * .095f + z * .075f);
    return unit({-dx, 1, -dz});
}
inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
struct ChunkKey {
    int x, z;
    bool operator<(const ChunkKey &b) const { return x != b.x ? x < b.x : z < b.z; }
    bool operator==(const ChunkKey &b) const { return x == b.x && z == b.z; }
};
constexpr float chunkSize = 32;
inline ChunkKey chunkAt(V3 p) {
    return {int(std::floor(p.x / chunkSize)), int(std::floor(p.z / chunkSize))};
}
struct Chunk {
    std::vector<Object> objects;
    ObjectRegion bounds;
    int trees = 0, grass = 0, structures = 0;
};
struct WorldStats {
    int chunks = 0, pending = 0, generated = 0, unloaded = 0, trees = 0, grass = 0, structures = 0;
    double generationMs = 0;
};
inline void addObject(std::vector<Object> &objects, MeshType mesh, V3 pos, V3 size, V3 color,
                      float rough = .85f, int lod = 0, float deform = 0, float pattern = 0) {
    objects.push_back(
        {pos,
         size * .5f,
         mesh,
         {transform(pos, size), {color.x, color.y, color.z, 1}, {rough, 0, pattern, deform}},
         lod});
}
inline Chunk generateChunk(ChunkKey key, uint32_t seed, bool detailed = false) {
    Chunk chunk;
    chunk.objects.reserve(detailed ? 3200 : 330);
    uint32_t state = hash32(seed ^ hash32(uint32_t(key.x)) ^ hash32(uint32_t(key.z) + 0x9e3779b9u));
    auto random = [&]() {
        state = hash32(state + 0x9e3779b9u);
        return float(state >> 8) * (1.f / 16777216.f);
    };
    float ox = key.x * chunkSize, oz = key.z * chunkSize;
    addObject(chunk.objects, MeshType::Terrain, {ox + 16, 0, oz + 16}, {32, 1, 32}, {.21f, .29f, .11f},
              .96f, 0, 1, 3);
    chunk.objects.back().half.y = 3.0f; // shader displacement bounded by +/-2.65 metres
    bool building = (state % 4) == 0 || (key.x == 0 && key.z == 0);
    V3 site{ox + 16, 0, oz + 17};
    site.y = terrainHeight(site.x, site.z);
    if (building) {
        chunk.structures = 1;
        float foundation = site.y + .75f;
        addObject(chunk.objects, MeshType::Cube, {site.x, foundation - .7f, site.z}, {8, 1.4f, 6},
                  {.40f, .39f, .31f}, .9f, 0, 0, 1);
        for (int side : {-1, 1})
            for (int end : {-1, 1})
                addObject(chunk.objects, MeshType::Cube,
                          {site.x + side * 3, foundation + 1.6f, site.z + end * 2}, {.4f, 3.2f, .4f},
                          {.16f, .085f, .035f});
        addObject(chunk.objects, MeshType::Cube, {site.x, foundation + 3.1f, site.z}, {6.8f, .3f, 4.8f},
                  {.22f, .13f, .055f});
        addObject(chunk.objects, MeshType::Roof, {site.x, foundation + 3.9f, site.z}, {8, 1.6f, 6},
                  {.30f, .115f, .055f});
        // A segmented rear wall: repeated components, shared geometry.
        for (int i = 0; i < 6; ++i)
            addObject(chunk.objects, MeshType::Cube, {site.x - 2.5f + i, foundation + 1, site.z - 2},
                      {.92f, 2, .25f}, {.29f, .19f, .09f});
    }
    for (int i = 0; i < 14; ++i) {
        float x = ox + 1 + random() * 30, z = oz + 1 + random() * 30;
        if ((building && std::abs(x - site.x) < 6 && std::abs(z - site.z) < 5) || (x * x + z * z < 110))
            continue;
        float h = 3.5f + random() * 3.5f, base = terrainHeight(x, z), spread = 2.6f + random() * 1.8f;
        V3 green{.07f + random() * .04f, .17f + random() * .08f, .035f};
        if (detailed) {
            const uint32_t treeSeed = hash32(state ^ 0xa511e9b3u);
            appendTree(chunk.objects, treeSeed % 3 == 0 ? TreeSpecies::ScotsPine : TreeSpecies::Spruce,
                       {x, base, z}, h, treeSeed, terrainHeight);
        } else {
            addObject(chunk.objects, MeshType::Cube, {x, base + h * .45f, z}, {.26f, h * .9f, .26f},
                      {.13f, .08f, .035f});
            addObject(chunk.objects, MeshType::Foliage, {x, base + h * .72f, z}, {spread, h * .7f, spread},
                      green, .95f, 1);
            addObject(chunk.objects, MeshType::Foliage, {x + spread * .24f, base + h * .57f, z + .3f},
                      {spread * .85f, h * .48f, spread * .8f}, green, .95f, 1);
        }
        ++chunk.trees;
    }
    // Grid-stratified placement avoids dense accidental clumps; seed controls jitter.
    for (int zc = 0; zc < 16; ++zc)
        for (int xc = 0; xc < 16; ++xc) {
            float x = ox + (xc + .15f + random() * .7f) * 2, z = oz + (zc + .15f + random() * .7f) * 2;
            if (building && std::abs(x - site.x) < 5 && std::abs(z - site.z) < 4)
                continue;
            float height = .35f + random() * .4f;
            addObject(chunk.objects, MeshType::Grass, {x, terrainHeight(x, z) + height * .5f, z},
                      {.65f, height, .65f}, {.15f + random() * .05f, .28f + random() * .08f, .055f},
                      .95f, 2, 2);
            chunk.objects.back().half.x += .2f; // wind deformation bound
            if (detailed && xc % 4 == 1 && zc % 4 == 1) {
                const uint32_t plantSeed = hash32(state ^ 0x5bca932du);
                const auto plant = static_cast<GroundPlant>(plantSeed % 3);
                appendGroundPlant(chunk.objects, plant, {x, terrainHeight(x, z), z},
                                  .55f + height * .66f, plantSeed, terrainHeight);
            }
            ++chunk.grass;
        }
    if (detailed) {
        // Upgrade materials/geometry while keeping seed-based layouts comparable.
        for (auto &o : chunk.objects) {
            if (o.mesh == MeshType::Terrain) {
                o.gpu.surface.z = 4;
                o.gpu.color = {.16f, .22f, .065f, 1};
            } else if (o.mesh == MeshType::Grass) {
                o.mesh = MeshType::Meadow;
                o.gpu.surface.z = 8;
                o.gpu.model.at(0, 0) *= 2.0f;
                o.gpu.model.at(2, 2) *= 2.0f;
                o.half.x = .85f;
                o.half.z = .65f;
            } else if (o.mesh == MeshType::Roof) {
                o.gpu.surface.z = 6;
            } else if (o.mesh == MeshType::Cube && o.gpu.surface.z < .5f) {
                o.gpu.surface.z = 5;
            }
        }
        if (building) {
            float floor = site.y + .75f;
            for (int side : {-1, 1}) {
                // Two handrails, posts, raised trims, exposed rafters and stone approach.
                addObject(chunk.objects, MeshType::Cube, {site.x + side * 3, floor + 1.0f, site.z},
                          {.15f, .18f, 4.0f}, {.18f, .095f, .035f}, .8f, 0, 0, 5);
                for (int k = 0; k < 9; ++k)
                    addObject(chunk.objects, MeshType::Cube,
                              {site.x + side * 3, floor + .5f, site.z - 1.8f + k * .45f},
                              {.075f, 1, .075f}, {.20f, .12f, .045f}, .8f, 0, 0, 5);
            }
            for (int k = 0; k < 7; ++k)
                addObject(chunk.objects, MeshType::Cube, {site.x - 3 + k, floor + 3.08f, site.z},
                          {.12f, .22f, 5.25f}, {.13f, .065f, .024f}, .8f, 0, 0, 5);
            for (int k = 0; k < 3; ++k)
                addObject(chunk.objects, MeshType::Cube,
                          {site.x, floor - .2f - k * .22f, site.z - 3.1f - k * .48f}, {3.6f, .35f, .70f},
                          {.34f, .32f, .25f}, .9f, 0, 0, 1);
        }
        for (int i = 0; i < 9; ++i) {
            float x = ox + random() * 32, z = oz + random() * 32;
            if (building && std::abs(x - site.x) < 6 && std::abs(z - site.z) < 5)
                continue;
            float size = .45f + random() * 1.1f;
            addObject(chunk.objects, MeshType::Rock, {x, terrainHeight(x, z) + size * .15f, z},
                      {size, size * .65f, size * .85f}, {.25f, .27f, .23f}, .9f, 0, 0, 9);
        }
    }
    chunk.bounds = objectRegion(chunk.objects, 0, chunk.objects.size());
    return chunk;
}
class ProceduralWorld {
    std::map<ChunkKey, Chunk> chunks;
    std::vector<ChunkKey> pending;
    ChunkKey center{100000000, 100000000};
    uint32_t seed;
    int radius;
    bool detailed;
    uint64_t revision = 0;

  public:
    explicit ProceduralWorld(uint32_t worldSeed = 12345, int viewRadius = 3,
                             bool visualReference = false)
        : seed(worldSeed), radius(viewRadius), detailed(visualReference) {
        if (radius < 1 || radius > 4)
            throw std::invalid_argument("World radius must be 1..4");
    }
    WorldStats update(V3 focus, Scene &scene, int maxChunks = 2, double budgetMs = 2.0) {
        if (maxChunks < 1 || budgetMs <= 0)
            throw std::invalid_argument("Invalid generation budget");
        auto start = std::chrono::steady_clock::now();
        WorldStats stats;
        bool changed = false;
        auto wanted = chunkAt(focus);
        if (!(wanted == center)) {
            center = wanted;
            pending.clear();
            // One extra ring of hysteresis prevents unload/reload oscillation at a boundary.
            for (auto it = chunks.begin(); it != chunks.end();) {
                if (std::abs(it->first.x - center.x) > radius + 1 ||
                    std::abs(it->first.z - center.z) > radius + 1) {
                    it = chunks.erase(it);
                    ++stats.unloaded;
                    changed = true;
                } else
                    ++it;
            }
            for (int z = -radius; z <= radius; ++z)
                for (int x = -radius; x <= radius; ++x) {
                    ChunkKey k{center.x + x, center.z + z};
                    if (chunks.find(k) == chunks.end())
                        pending.push_back(k);
                }
            std::sort(pending.begin(), pending.end(), [&](ChunkKey a, ChunkKey b) {
                int da = (a.x - center.x) * (a.x - center.x) + (a.z - center.z) * (a.z - center.z),
                    db = (b.x - center.x) * (b.x - center.x) + (b.z - center.z) * (b.z - center.z);
                return da != db ? da > db : b < a; // nearest is popped first; deterministic tie break
            });
        }
        while (!pending.empty() && stats.generated < maxChunks) {
            if (stats.generated > 0 &&
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                        .count() >= budgetMs)
                break;
            auto k = pending.back();
            pending.pop_back();
            chunks.emplace(k, generateChunk(k, seed, detailed));
            ++stats.generated;
            changed = true;
        }
        stats.generationMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (changed) {
            scene.objects.clear();
            scene.regions.clear();
            scene.objects.reserve(chunks.size() * 330);
            for (const auto &pair : chunks) {
                auto region = pair.second.bounds;
                region.begin = scene.objects.size();
                scene.objects.insert(scene.objects.end(), pair.second.objects.begin(),
                                     pair.second.objects.end());
                region.end = scene.objects.size();
                scene.regions.push_back(region);
            }
            scene.revision = ++revision;
        }
        scene.dynamicWorld = true;
        scene.visualReference = detailed;
        stats.chunks = int(chunks.size());
        stats.pending = int(pending.size());
        for (const auto &pair : chunks) {
            stats.trees += pair.second.trees;
            stats.grass += pair.second.grass;
            stats.structures += pair.second.structures;
        }
        stats.generationMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return stats;
    }
    int capacityLimit() const { return (2 * radius + 3) * (2 * radius + 3); }
};
inline M4 rotateX(float angle) {
    auto m = M4::identity();
    float c = std::cos(angle), s = std::sin(angle);
    m.at(1, 1) = c;
    m.at(1, 2) = -s;
    m.at(2, 1) = s;
    m.at(2, 2) = c;
    return m;
}
inline void appendActorPart(Scene &scene, const M4 &pose, V3 size, V3 color) {
    M4 m = pose * transform({}, size);
    V3 p{m.at(0, 3), m.at(1, 3), m.at(2, 3)};
    V3 h{(std::abs(m.at(0, 0)) + std::abs(m.at(0, 1)) + std::abs(m.at(0, 2))) * .5f,
         (std::abs(m.at(1, 0)) + std::abs(m.at(1, 1)) + std::abs(m.at(1, 2))) * .5f,
         (std::abs(m.at(2, 0)) + std::abs(m.at(2, 1)) + std::abs(m.at(2, 2))) * .5f};
    scene.objects.push_back({p, h, MeshType::Cube, {m, {color.x, color.y, color.z, 1}, {.8f, 0, 0, 0}}});
}
inline void appendActors(Scene &scene, double time, int count) {
    for (int i = 0; i < count; ++i) {
        float phase = float(time * 4 + i * .7), x = 4 + (i % 6) * 7.f,
              z = 3 + (i / 6) * 8.f + std::sin(float(time * .45 + i)) * 2.5f;
        // Avoid the first pavilion footprint. These are animation probes, not navigation agents.
        if (x > 10 && x < 22 && z > 11 && z < 23)
            x += 18;
        M4 root = transform({x, terrainHeight(x, z), z}, {1, 1, 1});
        V3 cloth{.08f + .03f * (i % 4), .20f, .38f}, skin{.62f, .38f, .21f}, boots{.045f, .05f, .065f};
        auto part = [&](M4 pose, V3 size, V3 color) {
            appendActorPart(scene, root * pose, size, color);
        };
        part(transform({0, 1.1f, 0}, {1, 1, 1}), {.46f, .6f, .27f}, cloth);
        part(transform({0, 1.64f, 0}, {1, 1, 1}), {.27f, .32f, .27f}, skin);
        for (int side : {-1, 1}) {
            float swing = std::sin(phase + (side == 1 ? pi : 0)) * .45f;
            auto thigh = transform({side * .14f, .82f, 0}, {1, 1, 1}) * rotateX(swing);
            part(thigh * transform({0, -.21f, 0}, {1, 1, 1}), {.17f, .42f, .18f}, cloth);
            auto knee = thigh * transform({0, -.42f, 0}, {1, 1, 1}) *
                        rotateX(std::max(0.f, -std::sin(phase + (side == 1 ? pi : 0))) * .65f);
            part(knee * transform({0, -.17f, 0}, {1, 1, 1}), {.16f, .34f, .17f}, boots);
            part(knee * transform({0, -.35f, .06f}, {1, 1, 1}), {.19f, .12f, .30f}, boots);
            auto arm = transform({side * .32f, 1.36f, 0}, {1, 1, 1}) * rotateX(-swing);
            part(arm * transform({0, -.28f, 0}, {1, 1, 1}), {.13f, .56f, .14f}, cloth);
        }
    }
}
inline Camera worldTour(double seconds, bool strategy) {
    Camera c;
    c.strategy = strategy;
    float x = 8 + float(seconds) * 7, z = 6 + std::sin(float(seconds) * .18f) * 18;
    c.target = {x, terrainHeight(x, z), z};
    c.distance = 45;
    c.pos = {x, terrainHeight(x, z) + 3.2f, z};
    c.face({x + 25, terrainHeight(x + 25, z + 12) + 2, z + 12});
    return c;
}
} // namespace pl
