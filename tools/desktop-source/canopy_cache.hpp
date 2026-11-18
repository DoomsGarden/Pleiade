#pragma once
#include "canopy_layers.hpp"
namespace pl {
struct CanopySelection {
    std::array<float, 3> weights{};
    std::array<CanopyView, 4> views{};
};
inline CanopySelection selectCanopy(V3 eye, float focal, V3 center, const M4 &m) {
    V3 scale{length({m.at(0, 0), m.at(1, 0), m.at(2, 0)}), length({m.at(0, 1), m.at(1, 1), m.at(2, 1)}),
             length({m.at(0, 2), m.at(1, 2), m.at(2, 2)})};
    float pixels =
        .9f * std::max({scale.x, scale.y, scale.z}) * focal / std::max(length(center - eye), .1f);
    CanopySelection s;
    s.weights = canopyWeights(pixels);
    if (s.weights[2] > .00001f) {
        V3 d = eye - center;
        s.views = canopyViews({dot(d, {m.at(0, 0), m.at(1, 0), m.at(2, 0)}) / (scale.x * scale.x),
                               dot(d, {m.at(0, 1), m.at(1, 1), m.at(2, 1)}) / (scale.y * scale.y),
                               dot(d, {m.at(0, 2), m.at(1, 2), m.at(2, 2)}) / (scale.z * scale.z)});
    }
    return s;
}
class CanopyCache {
    struct Entry {
        uint64_t epoch = 0;
        V3 center{};
        M4 model{};
        CanopySelection selection{};
    };
    std::vector<int> indices;
    std::vector<Entry> entries;
    uint64_t revision = ~uint64_t(0), epoch = 1;
    V3 eye{};
    float focal = -1;
    static bool equal(V3 a, V3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

  public:
    int hits = 0, misses = 0;
    void begin(uint64_t rev, size_t count, V3 cameraEye, float cameraFocal) {
        hits = misses = 0;
        if (rev != revision || count != indices.size()) {
            indices.assign(count, -1);
            entries.clear();
            revision = rev;
        }
        if (!equal(eye, cameraEye) || focal != cameraFocal) {
            ++epoch;
            eye = cameraEye;
            focal = cameraFocal;
        }
    }
    const CanopySelection &get(size_t index, V3 center, const M4 &model) {
        int &slot = indices.at(index);
        if (slot < 0) {
            slot = int(entries.size());
            entries.emplace_back();
        }
        auto &e = entries[slot];
        if (e.epoch == epoch && equal(e.center, center) && e.model.v == model.v) {
            ++hits;
            return e.selection;
        }
        ++misses;
        e.epoch = epoch;
        e.center = center;
        e.model = model;
        e.selection = selectCanopy(eye, focal, center, model);
        return e.selection;
    }
    size_t bytes() const {
        return indices.capacity() * sizeof(int) + entries.capacity() * sizeof(Entry);
    }
};
} // namespace pl
