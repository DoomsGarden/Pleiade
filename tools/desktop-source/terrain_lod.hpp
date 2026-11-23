#pragma once
#include "math.hpp"
#include <cstdint>
namespace pl {
struct TerrainTopology {
    std::vector<V3> positions;
    std::vector<uint32_t> indices;
};
inline TerrainTopology coarseTerrainTopology() {
    TerrainTopology mesh;
    std::array<int, 289> ids;
    ids.fill(-1);
    auto vertex = [&](int x, int z) {
        int &id = ids[z * 17 + x];
        if (id < 0) {
            id = int(mesh.positions.size());
            mesh.positions.push_back({x / 16.f - .5f, 0, z / 16.f - .5f});
        }
        return uint32_t(id);
    };
    for (int z = 0; z < 16; z += 2)
        for (int x = 0; x < 16; x += 2) {
            auto center = vertex(x + 1, z + 1);
            std::vector<uint32_t> ring;
            for (int k = 0; k < 2; k += (x == 0 ? 1 : 2))
                ring.push_back(vertex(x, z + k));
            for (int k = 0; k < 2; k += (z == 14 ? 1 : 2))
                ring.push_back(vertex(x + k, z + 2));
            for (int k = 0; k < 2; k += (x == 14 ? 1 : 2))
                ring.push_back(vertex(x + 2, z + 2 - k));
            for (int k = 0; k < 2; k += (z == 0 ? 1 : 2))
                ring.push_back(vertex(x + 2 - k, z));
            for (size_t i = 0; i < ring.size(); ++i)
                mesh.indices.insert(mesh.indices.end(), {center, ring[i], ring[(i + 1) % ring.size()]});
        }
    return mesh;
}
// Current analytic height field: Hessian spectral norm <= .0172975 / metre.
// Fine/coarse interpolation difference <= .5*H*(2+4) = .0518925 m.
constexpr float terrainHeightDifferenceBound = .052f;
inline bool useCoarseTerrain(const Camera &camera, float viewportHeight, V3 center, V3 half) {
    M4 view = camera.view();
    auto p = view * V4{center.x, center.y, center.z, 1};
    float nearestDepth = -p.z - length(half);
    if (nearestDepth <= terrainHeightDifferenceBound)
        return false;
    float halfFov = camera.verticalFov() * .5f;
    float focal = viewportHeight / (2 * std::tan(halfFov));
    // Conservative factor for vertical displacement affecting both view Y and Z.
    float projectedError =
        2 * terrainHeightDifferenceBound * focal / (nearestDepth - terrainHeightDifferenceBound);
    return projectedError <= .75f;
}
} // namespace pl
