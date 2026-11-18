#pragma once
#include "world.hpp"
#include <queue>
namespace pl {
// Bounded exploration prototype: 256 m square, 2 m cells, one observer.
struct VisibilityMap {
    static constexpr int size = 128;
    static constexpr float cell = 2.f, origin = -128.f;
    std::vector<float> ground, obstacle;
    std::vector<unsigned char> pixels;
    std::vector<uint32_t> exploredSums;
    uint64_t revision = 0, worldRevision = ~uint64_t(0);
    int visibleCount = 0, exploredCount = 0, rays = 0;
    VisibilityMap()
        : ground(size * size), obstacle(size * size), pixels(size * size * 2, 0),
          exploredSums((size + 1) * (size + 1), 0) {
        for (int z = 0; z < size; ++z)
            for (int x = 0; x < size; ++x) {
                auto p = position(z * size + x);
                ground[z * size + x] = terrainHeight(p.x, p.z);
            }
        obstacle = ground;
    }
    int index(float x, float z) const {
        int ix = int(std::floor((x - origin) / cell)), iz = int(std::floor((z - origin) / cell));
        return ix >= 0 && ix < size && iz >= 0 && iz < size ? iz * size + ix : -1;
    }
    V3 position(int i) const {
        return {origin + (i % size + .5f) * cell, 0, origin + (i / size + .5f) * cell};
    }
    void rebuild(const Scene &s) {
        if (worldRevision == s.revision)
            return;
        worldRevision = s.revision;
        obstacle = ground;
        for (const auto &o : s.objects) {
            if (o.mesh != MeshType::Cube && o.mesh != MeshType::Roof)
                continue;
            auto lo = o.center - o.half, hi = o.center + o.half;
            int x0 = std::max(0, int(std::floor((lo.x - origin) / cell))),
                x1 = std::min(size - 1, int(std::floor((hi.x - origin) / cell)));
            int z0 = std::max(0, int(std::floor((lo.z - origin) / cell))),
                z1 = std::min(size - 1, int(std::floor((hi.z - origin) / cell)));
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    obstacle[z * size + x] = std::max(obstacle[z * size + x], hi.y);
        }
    }
    bool walkable(int i) const { return i >= 0 && i < size * size && obstacle[i] - ground[i] < .8f; }
    int nearest(V3 p) const {
        int best = -1;
        float distance = 1e30f;
        for (int i = 0; i < size * size; ++i)
            if (walkable(i)) {
                auto q = position(i);
                float d = (q.x - p.x) * (q.x - p.x) + (q.z - p.z) * (q.z - p.z);
                if (d < distance) {
                    distance = d;
                    best = i;
                }
            }
        return best;
    }
    bool lineOfSight(V3 eye, V3 target) const {
        V3 d = target - eye;
        int steps = std::max(1, int(std::ceil(length(d) / .75f)));
        for (int k = 1; k < steps; ++k) {
            auto p = eye + d * (float(k) / steps);
            int i = index(p.x, p.z);
            if (i < 0 || obstacle[i] > p.y + .15f)
                return false;
        }
        return true;
    }
    void reveal(V3 observer, float radius) {
        for (size_t i = 0; i < pixels.size(); i += 2)
            pixels[i] = 0;
        visibleCount = exploredCount = rays = 0;
        V3 eye = observer + V3{0, 1.7f, 0};
        int center = index(observer.x, observer.z);
        if (center >= 0) {
            int r = int(std::ceil(radius / cell));
            for (int z = std::max(0, center / size - r); z <= std::min(size - 1, center / size + r); ++z)
                for (int x = std::max(0, center % size - r); x <= std::min(size - 1, center % size + r);
                     ++x) {
                    int i = z * size + x;
                    auto p = position(i);
                    float dx = p.x - observer.x, dz = p.z - observer.z;
                    if (dx * dx + dz * dz > radius * radius)
                        continue;
                    ++rays;
                    p.y = std::max(ground[i] + .8f, obstacle[i] + .1f);
                    if (i == center || lineOfSight(eye, p))
                        pixels[i * 2] = pixels[i * 2 + 1] = 255;
                }
        }
        for (int i = 0; i < size * size; ++i) {
            visibleCount += pixels[i * 2] != 0;
            exploredCount += pixels[i * 2 + 1] != 0;
        }
        rebuildExploredSums();
        ++revision;
    }
    void rebuildExploredSums() {
        // CPU-only summed-area table; fixed allocation, refreshed at visibility tick rate.
        for (int z = 0; z < size; ++z) {
            uint32_t row = 0;
            for (int x = 0; x < size; ++x) {
                row += pixels[(z * size + x) * 2 + 1] != 0;
                exploredSums[(z + 1) * (size + 1) + x + 1] = exploredSums[z * (size + 1) + x + 1] + row;
            }
        }
    }
    bool exploredBounds(V3 p, V3 h) const {
        int x0 = std::max(0, int(std::floor((p.x - h.x - origin) / cell))),
            x1 = std::min(size - 1, int(std::floor((p.x + h.x - origin) / cell)));
        int z0 = std::max(0, int(std::floor((p.z - h.z - origin) / cell))),
            z1 = std::min(size - 1, int(std::floor((p.z + h.z - origin) / cell)));
        if (x0 > x1 || z0 > z1)
            return false;
        const int stride = size + 1;
        return exploredSums[(z1 + 1) * stride + x1 + 1] + exploredSums[z0 * stride + x0] >
               exploredSums[z0 * stride + x1 + 1] + exploredSums[(z1 + 1) * stride + x0];
    }
};
struct Scout {
    V3 position{8, 0, 6}, heading{0, 0, 1};
    std::vector<int> path;
    size_t step = 0;
    int goal = 0;
    bool initialized = false;
    void plan(const VisibilityMap &map) {
        const V3 goals[] = {{-30, 0, -20}, {35, 0, -20}, {35, 0, 30}, {-30, 0, 30}};
        int start = map.nearest(position), end = map.nearest(goals[goal++ % 4]);
        path.clear();
        step = 0;
        if (start < 0 || end < 0)
            return;
        std::vector<int> previous(VisibilityMap::size * VisibilityMap::size, -1), queue;
        queue.reserve(previous.size());
        queue.push_back(start);
        previous[start] = start;
        for (size_t k = 0; k < queue.size() && previous[end] < 0; ++k) {
            int i = queue[k], x = i % VisibilityMap::size, z = i / VisibilityMap::size;
            for (auto d : {std::pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                int nx = x + d.first, nz = z + d.second;
                if (nx < 0 || nx >= VisibilityMap::size || nz < 0 || nz >= VisibilityMap::size)
                    continue;
                int j = nz * VisibilityMap::size + nx;
                if (previous[j] >= 0 || !map.walkable(j) ||
                    std::abs(map.ground[j] - map.ground[i]) > 1.2f)
                    continue;
                previous[j] = i;
                queue.push_back(j);
            }
        }
        if (previous[end] < 0)
            return;
        for (int i = end; i != start; i = previous[i])
            path.push_back(i);
        std::reverse(path.begin(), path.end());
    }
    void advance(const VisibilityMap &map, double dt) {
        if (!initialized) {
            int i = map.nearest(position);
            if (i < 0)
                return;
            position = map.position(i);
            initialized = true;
        }
        if (step >= path.size())
            plan(map);
        float remaining = float(std::min(dt, .1)) * 4.f;
        while (remaining > 0 && step < path.size()) {
            if (!map.walkable(path[step])) {
                path.clear();
                break;
            }
            auto target = map.position(path[step]);
            auto d = target - position;
            d.y = 0;
            float n = length(d);
            if (n > 1e-5f)
                heading = d * (1 / n);
            if (n <= remaining) {
                position = target;
                remaining -= n;
                ++step;
            } else {
                position = position + d * (remaining / n);
                remaining = 0;
            }
        }
        position.y = terrainHeight(position.x, position.z);
    }
    void append(Scene &s, double time) const {
        size_t begin = s.objects.size();
        appendActors(s, time, 1);
        V3 old{4, terrainHeight(4, 3 + std::sin(float(time * .45)) * 2.5f),
               3 + std::sin(float(time * .45)) * 2.5f};
        M4 rotation = M4::identity();
        float cs = heading.z, sn = heading.x;
        rotation.at(0, 0) = cs;
        rotation.at(0, 2) = sn;
        rotation.at(2, 0) = -sn;
        rotation.at(2, 2) = cs;
        M4 root = transform(position, {1, 1, 1}) * rotation * transform(old * -1.f, {1, 1, 1});
        for (size_t i = begin; i < s.objects.size(); ++i) {
            auto &o = s.objects[i];
            auto c = root * V4{o.center.x, o.center.y, o.center.z, 1};
            o.center = {c.x, c.y, c.z};
            o.half = {std::abs(cs) * o.half.x + std::abs(sn) * o.half.z, o.half.y,
                      std::abs(sn) * o.half.x + std::abs(cs) * o.half.z};
            o.gpu.model = root * o.gpu.model;
            o.gpu.color = {.08f, .55f, .95f, 1};
        }
    }
};
} // namespace pl
