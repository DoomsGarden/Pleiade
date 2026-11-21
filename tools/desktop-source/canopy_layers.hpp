#pragma once
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
namespace pl {
struct LeafTriangle {
    V3 a, b, c;
};
inline std::vector<LeafTriangle> detailedLeaves() {
    std::vector<LeafTriangle> out;
    for (int i = 0; i < 150; ++i) {
        float f = (i + .5f) / 150, y = 1 - 2 * f, a = i * 2.39996323f,
              r = std::sqrt(std::max(0.f, 1 - y * y));
        V3 dir{r * std::cos(a), y, r * std::sin(a)}, center = dir * (.30f + .065f * std::sin(i * 7.31f));
        V3 normal = unit(dir + V3{.2f, .65f, .1f}),
           u = unit(cross(std::abs(normal.y) > .95f ? V3{1, 0, 0} : V3{0, 1, 0}, normal)),
           v = cross(normal, u);
        // Four smaller curved leaflets per cluster, all fixed in object space.
        for (int k = 0; k < 4; ++k) {
            V3 c = center + u * ((k & 1) ? .035f : -.035f) + v * ((k & 2) ? .019f : -.019f);
            float angle = .35f * std::sin(i * 3.7f + k * 2.1f);
            V3 a = u * std::cos(angle) + v * std::sin(angle),
               b = v * std::cos(angle) - u * std::sin(angle);
            V3 outline[6] = {c - a * .040f, c - a * .018f + b * .019f, c + a * .018f + b * .019f,
                             c + a * .040f, c + a * .018f - b * .019f, c - a * .018f - b * .019f};
            V3 ridge = c + normal * .004f;
            for (int j = 0; j < 6; ++j)
                out.push_back({ridge, outline[j], outline[(j + 1) % 6]});
        }
    }
    return out;
}
constexpr int canopyAzimuths = 16, canopyElevations = 5, canopyTile = 64;
struct CanopyBasis {
    V3 right, up, direction;
};
inline CanopyBasis canopyBasis(int index) {
    float az = (index % canopyAzimuths) * 2 * pi / canopyAzimuths;
    float el = -pi * .5f + (index / canopyAzimuths) * pi / (canopyElevations - 1);
    V3 d{std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az)},
        r{std::cos(az), 0, -std::sin(az)};
    return {r, cross(d, r), d};
}
struct CanopyView {
    int index;
    float weight;
};
inline std::array<CanopyView, 4> canopyViews(V3 direction) {
    auto d = unit(direction);
    float az = std::atan2(d.x, d.z) * canopyAzimuths / (2 * pi);
    if (az < 0)
        az += canopyAzimuths;
    float el = (std::asin(std::clamp(d.y, -1.f, 1.f)) + pi * .5f) * (canopyElevations - 1) / pi;
    int a = int(std::floor(az)) % canopyAzimuths, e = std::min(int(el), canopyElevations - 2);
    float x = az - std::floor(az), y = el - e;
    return {{{e * canopyAzimuths + a, (1 - x) * (1 - y)},
             {e * canopyAzimuths + (a + 1) % canopyAzimuths, x * (1 - y)},
             {(e + 1) * canopyAzimuths + a, (1 - x) * y},
             {(e + 1) * canopyAzimuths + (a + 1) % canopyAzimuths, x * y}}};
}
inline std::array<float, 3> canopyWeights(float pixels) {
    auto smooth = [](float a, float b, float x) {
        float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
        return t * t * (3 - 2 * t);
    };
    float nearWeight = smooth(90, 120, pixels), middle = smooth(30, 45, pixels);
    return {nearWeight, (1 - nearWeight) * middle, (1 - nearWeight) * (1 - middle)};
}
inline float canopyPixels(const Camera &camera, float height, V3 center, V3 scale) {
    const V3 toCenter=center-camera.eye();
    const V3 forward=camera.strategy?unit(camera.target-camera.eye()):camera.forward();
    float distance=std::max({dot(toCenter,forward),length(toCenter)*.72f,.1f}),
          diameter = .9f * std::max({scale.x, scale.y, scale.z});
    float focal = height / (2 * std::tan((camera.strategy ? 48.f : 62.f) * pi / 360));
    return diameter * focal / distance;
}
// RGBA = object-space normal (signed float), nearest surface depth + 0.5; empty alpha=0.
inline std::vector<float> bakeCanopyAtlas() {
    constexpr int width = canopyAzimuths * canopyTile, height = canopyElevations * canopyTile;
    std::vector<float> data(width * height * 4, 0.f);
    auto triangles = detailedLeaves();
    for (int index = 0; index < canopyAzimuths * canopyElevations; ++index) {
        auto basis = canopyBasis(index);
        for (auto t : triangles) {
            V3 p[3] = {t.a, t.b, t.c};
            float x[3], y[3], z[3];
            for (int j = 0; j < 3; ++j) {
                x[j] = (dot(p[j], basis.right) + .5f) * canopyTile;
                y[j] = (dot(p[j], basis.up) + .5f) * canopyTile;
                z[j] = dot(p[j], basis.direction) + .5f;
            }
            float den = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2]);
            if (std::abs(den) < 1e-7f)
                continue;
            V3 n = unit(cross(t.b - t.a, t.c - t.a));
            if (dot(n, basis.direction) < 0)
                n = n * -1;
            int x0 = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]})))),
                x1 = std::min(canopyTile - 1, int(std::ceil(std::max({x[0], x[1], x[2]}))));
            int y0 = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]})))),
                y1 = std::min(canopyTile - 1, int(std::ceil(std::max({y[0], y[1], y[2]}))));
            for (int iy = y0; iy <= y1; ++iy)
                for (int ix = x0; ix <= x1; ++ix) {
                    float a =
                        ((y[1] - y[2]) * (ix + .5f - x[2]) + (x[2] - x[1]) * (iy + .5f - y[2])) / den;
                    float b = ((y[2] - y[0]) * (ix + .5f - x[2]) + (x[0] - x[2]) * (iy + .5f - y[2])) /
                              den,
                          c = 1 - a - b;
                    if (a < 0 || b < 0 || c < 0)
                        continue;
                    float depth = a * z[0] + b * z[1] + c * z[2];
                    int o = ((index / canopyAzimuths * canopyTile + iy) * width +
                             index % canopyAzimuths * canopyTile + ix) *
                            4;
                    if (depth > data[o + 3]) {
                        data[o] = n.x;
                        data[o + 1] = n.y;
                        data[o + 2] = n.z;
                        data[o + 3] = depth;
                    }
                }
        }
    }
    return data;
}
} // namespace pl
