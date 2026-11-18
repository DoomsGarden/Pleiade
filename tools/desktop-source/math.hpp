#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace pl {
constexpr float pi = 3.14159265358979323846f;
struct V3 {
    float x = 0, y = 0, z = 0;
};
inline V3 operator+(V3 a, V3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline V3 operator-(V3 a, V3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline V3 operator*(V3 a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
inline float dot(V3 a, V3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V3 cross(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(V3 v) {
    return std::sqrt(dot(v, v));
}
inline V3 unit(V3 v) {
    float l = length(v);
    return l > 1e-8f ? v * (1 / l) : V3{};
}
struct V4 {
    float x, y, z, w;
};
struct M4 {
    std::array<float, 16> v{}; // column-major; column vectors, right-handed, OpenGL -1..1 depth
    float &at(int r, int c) { return v[c * 4 + r]; }
    float at(int r, int c) const { return v[c * 4 + r]; }
    static M4 identity() {
        M4 m;
        for (int i = 0; i < 4; ++i)
            m.at(i, i) = 1;
        return m;
    }
};
inline M4 operator*(const M4 &a, const M4 &b) {
    M4 m;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 4; ++k)
                m.at(r, c) += a.at(r, k) * b.at(k, c);
    return m;
}
inline V4 operator*(const M4 &a, V4 b) {
    float v[] = {b.x, b.y, b.z, b.w};
    float out[4]{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            out[r] += a.at(r, c) * v[c];
    return {out[0], out[1], out[2], out[3]};
}
inline M4 transform(V3 p, V3 s) {
    M4 m = M4::identity();
    m.at(0, 0) = s.x;
    m.at(1, 1) = s.y;
    m.at(2, 2) = s.z;
    m.at(0, 3) = p.x;
    m.at(1, 3) = p.y;
    m.at(2, 3) = p.z;
    return m;
}
inline M4 perspective(float fov, float aspect, float nearZ, float farZ) {
    if (!(fov > 0 && fov < pi && aspect > 0 && nearZ > 0 && farZ > nearZ))
        throw std::invalid_argument("Invalid perspective");
    M4 m;
    float f = 1 / std::tan(fov / 2);
    m.at(0, 0) = f / aspect;
    m.at(1, 1) = f;
    m.at(2, 2) = (farZ + nearZ) / (nearZ - farZ);
    m.at(2, 3) = 2 * farZ * nearZ / (nearZ - farZ);
    m.at(3, 2) = -1;
    return m;
}
inline M4 ortho(float l, float r, float b, float t, float n, float f) {
    M4 m = M4::identity();
    m.at(0, 0) = 2 / (r - l);
    m.at(1, 1) = 2 / (t - b);
    m.at(2, 2) = -2 / (f - n);
    m.at(0, 3) = -(r + l) / (r - l);
    m.at(1, 3) = -(t + b) / (t - b);
    m.at(2, 3) = -(f + n) / (f - n);
    return m;
}
inline M4 lookAt(V3 eye, V3 target, V3 up = {0, 1, 0}) {
    V3 f = unit(target - eye), s = unit(cross(f, up)), u = cross(s, f);
    M4 m = M4::identity();
    m.at(0, 0) = s.x;
    m.at(0, 1) = s.y;
    m.at(0, 2) = s.z;
    m.at(0, 3) = -dot(s, eye);
    m.at(1, 0) = u.x;
    m.at(1, 1) = u.y;
    m.at(1, 2) = u.z;
    m.at(1, 3) = -dot(u, eye);
    m.at(2, 0) = -f.x;
    m.at(2, 1) = -f.y;
    m.at(2, 2) = -f.z;
    m.at(2, 3) = dot(f, eye);
    return m;
}
struct Frustum {
    std::array<V4, 6> planes;
    explicit Frustum(const M4 &m) {
        for (int a = 0; a < 3; ++a)
            for (int s = 0; s < 2; ++s) {
                float sign = s ? -1.f : 1.f;
                V4 p{m.at(3, 0) + sign * m.at(a, 0), m.at(3, 1) + sign * m.at(a, 1),
                     m.at(3, 2) + sign * m.at(a, 2), m.at(3, 3) + sign * m.at(a, 3)};
                float len = length({p.x, p.y, p.z});
                planes[a * 2 + s] = {p.x / len, p.y / len, p.z / len, p.w / len};
            }
    }
    bool visible(V3 center, V3 half) const {
        for (auto p : planes) {
            float radius = std::abs(p.x) * half.x + std::abs(p.y) * half.y + std::abs(p.z) * half.z;
            if (p.x * center.x + p.y * center.y + p.z * center.z + p.w < -radius - 1e-4f)
                return false;
        }
        return true;
    }
};
struct Camera {
    V3 pos{15, 7, 22};
    float yaw = -2.55f, pitch = -.22f;
    bool strategy = false;
    float distance = 39;
    float orbit = 0;
    V3 target{0, 0, 0};
    V3 forward() const {
        return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    }
    V3 eye() const {
        if (!strategy)
            return pos;
        V3 offset{distance * .58f, distance * .82f, distance * .72f};
        if (orbit == 0)
            return target + offset;
        return target + V3{offset.x * std::cos(orbit) + offset.z * std::sin(orbit), offset.y,
                           -offset.x * std::sin(orbit) + offset.z * std::cos(orbit)};
    }
    M4 view() const { return strategy ? lookAt(eye(), target) : lookAt(pos, pos + forward()); }
    M4 projection(float aspect) const {
        return perspective(strategy ? 48 * pi / 180 : 62 * pi / 180, aspect, .1f, 250.f);
    }
    void face(V3 at) {
        V3 f = unit(at - pos);
        pitch = std::asin(std::clamp(f.y, -1.f, 1.f));
        yaw = std::atan2(f.x, f.z);
    }
    void move(float right, float forwardInput, float dt, bool fast) {
        V3 f = strategy ? unit(V3{-.58f, 0, -.72f}) : V3{std::sin(yaw), 0, std::cos(yaw)};
        V3 r = unit(cross(f, {0, 1, 0}));
        V3 d = f * forwardInput + r * right;
        if (length(d) > 1)
            d = unit(d);
        d = d * (dt * (fast ? 16.f : 6.f));
        if (strategy)
            target = target + d;
        else
            pos = pos + d;
    }
};
inline void manualCameraMotion(Camera &camera, bool benchmark, float right, float forward,
                               float vertical, float dt, bool fast) {
    if (benchmark)
        return;
    camera.move(right, forward, dt, fast);
    if (!camera.strategy)
        camera.pos.y += vertical * dt * 5;
}
inline Camera tour(int frame, bool strategy) {
    Camera c;
    c.strategy = strategy;
    float t = frame / 60.f;
    if (strategy) {
        c.distance = 39 + 3 * std::sin(t * .2f);
        c.target = {2 * std::sin(t * .2f), 0, 0};
    } else {
        float a = .62f + t * .055f;
        c.pos = {21 * std::sin(a), 5.4f + std::sin(t * .22f), 21 * std::cos(a)};
        c.face({0, 2.4f, 0});
    }
    return c;
}
inline double percentile(std::vector<double> values, double p) {
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    auto i = static_cast<size_t>(std::ceil(std::clamp(p, 0., 1.) * values.size()));
    return values[i ? i - 1 : 0];
}
} // namespace pl
