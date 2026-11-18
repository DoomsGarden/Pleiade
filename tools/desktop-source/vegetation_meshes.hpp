#pragma once
#include "scene.hpp"

namespace pl {
inline constexpr std::array vegetationMeshes{
    MeshType::TreeWood, MeshType::SpruceNeedles, MeshType::PineNeedles, MeshType::JuniperNeedles,
    MeshType::FernFrond, MeshType::BellflowerStem, MeshType::BellflowerBloom,
    MeshType::SpruceNeedlesFar, MeshType::PineNeedlesFar, MeshType::JuniperNeedlesFar,
    MeshType::FernFrondFar};

inline MeshType vegetationFarMesh(MeshType mesh) {
    switch (mesh) {
    case MeshType::SpruceNeedles: return MeshType::SpruceNeedlesFar;
    case MeshType::PineNeedles: return MeshType::PineNeedlesFar;
    case MeshType::JuniperNeedles: return MeshType::JuniperNeedlesFar;
    case MeshType::FernFrond: return MeshType::FernFrondFar;
    default: return mesh;
    }
}

template <class Triangle>
void buildVegetationMesh(MeshType mesh, Triangle triangle) {
    auto blade = [&](V3 base, V3 middle, V3 tip, V3 side, float width) {
        V3 left = middle - side * width, right = middle + side * width;
        triangle(base, left, right, true);
        triangle(left, tip, right, true);
    };
    auto stem = [&](V3 from, V3 to, float width) {
        for (V3 side : {V3{width, 0, 0}, V3{0, 0, width}}) {
            triangle(from - side, to - side, to + side, true);
            triangle(from - side, to + side, from + side, true);
        }
    };
    if (mesh == MeshType::TreeWood) {
        auto ring = [](int i, float h, float radius) {
            const float a = i * (2 * pi / 12);
            const float ripple = i % 2 ? 1.f : .94f;
            return V3{std::cos(a) * radius * ripple, h, std::sin(a) * radius * ripple};
        };
        for (int i = 0; i < 12; ++i) {
            V3 a = ring(i, -.5f, .5f), b = ring(i + 1, -.5f, .5f);
            V3 c = ring(i, .5f, .394f), d = ring(i + 1, .5f, .394f);
            triangle(a, c, b, false);
            triangle(b, c, d, false);
            triangle({0, -.5f, 0}, a, b, false);
            triangle({0, .5f, 0}, d, c, false);
        }
    } else if (mesh == MeshType::SpruceNeedles || mesh == MeshType::SpruceNeedlesFar) {
        const bool far = mesh == MeshType::SpruceNeedlesFar;
        stem({0, -.5f, 0}, {0, .49f, 0}, .013f);
        for (int i = 0; i < 17; i += far ? 2 : 1) {
            const float t = i / 17.f;
            const float a = i * 2.39996f;
            const V3 from{0, -.44f + .87f * t, 0};
            const V3 radial{std::cos(a), 0, std::sin(a)};
            V3 branch = radial * (.45f * (1 - .77f * t)) + V3{0, .055f, 0};
            V3 side{-radial.z, 0, radial.x};
            if (far) {
                blade(from, from + branch * .53f, from + branch * 1.04f, side, .066f);
                blade(from, from + branch * .55f, from + branch, {0, 1, 0}, .052f);
                continue;
            }
            stem(from, from + branch, .004f);
            // Short, overlapping sprays retain coverage when individual needles are subpixel.
            blade(from + branch * .11f, from + branch * .52f, from + branch * .96f, side, .047f);
            blade(from + branch * .26f, from + branch * .61f, from + branch, {0, 1, 0}, .035f);
            for (int j = 0; j < 8; ++j) {
                V3 base = from + branch * (.13f + j * .103f);
                V3 needle = side * ((j % 2 ? 1 : -1) * (.095f - .005f * j)) + radial * .037f;
                needle.y = (j % 3 == 0 ? -.026f : .027f);
                blade(base, base + needle * .38f, base + needle, radial, .014f);
            }
        }
    } else if (mesh == MeshType::PineNeedles || mesh == MeshType::PineNeedlesFar ||
               mesh == MeshType::JuniperNeedles || mesh == MeshType::JuniperNeedlesFar) {
        const bool far = mesh == MeshType::PineNeedlesFar || mesh == MeshType::JuniperNeedlesFar;
        const bool pine = mesh == MeshType::PineNeedles || mesh == MeshType::PineNeedlesFar;
        stem({0, -.5f, 0}, {0, .46f, 0}, .013f);
        for (int i = 0; i < (pine ? 24 : 19); i += far ? 2 : 1) {
            const float t = i / (pine ? 24.f : 19.f);
            const float a = i * 2.39996f;
            const V3 from{0, -.40f + .72f * t, 0};
            const V3 radial{std::cos(a), 0, std::sin(a)};
            const V3 twig = radial * ((pine ? .28f : .34f) * (1 - t * .59f)) + V3{0, .036f, 0};
            const V3 tangent{-radial.z, 0, radial.x};
            if (far) {
                V3 tip = from + twig + radial * .09f + V3{0, pine ? .10f : .042f, 0};
                blade(from, from + twig * .75f + V3{0, .023f, 0}, tip, tangent, .069f);
                blade(from, from + twig * .78f, tip, {0, 1, 0}, .056f);
                continue;
            }
            stem(from, from + twig, .005f);
            if (pine) {
                V3 end = from + twig + radial * .04f + V3{0, .073f, 0};
                blade(from + twig * .41f, from + twig * .77f + V3{0, .027f, 0}, end, tangent, .048f);
            }
            for (int j = 0; j < 5; ++j) {
                V3 base = from + twig * (.24f + .155f * j);
                if (pine) {
                    for (float side : {-1.f, 1.f}) {
                        V3 tip = base + radial * .093f + tangent * (.049f * side) + V3{0, .10f, 0};
                        V3 mid = base + (tip - base) * .60f + V3{0, .021f, 0};
                        blade(base, mid, tip, tangent, .008f);
                    }
                } else {
                    for (int leaf = 0; leaf < 3; ++leaf) {
                        const float whorl = (leaf + j * .11f) * (2 * pi / 3);
                        V3 tip = base + radial * .051f + tangent * (.083f * std::cos(whorl)) +
                                 V3{0, .065f * std::sin(whorl), 0};
                        blade(base, base + (tip - base) * .45f, tip, radial, .012f);
                    }
                }
            }
        }
    } else if (mesh == MeshType::FernFrond || mesh == MeshType::FernFrondFar) {
        const bool far = mesh == MeshType::FernFrondFar;
        auto rachis = [](float t) { return V3{.22f * std::sin(pi * t) - .11f * t, t - .5f, 0}; };
        for (int i = 0; i < 17; i += far ? 2 : 1) {
            float t = i / 17.f;
            stem(rachis(t), rachis(std::min(1.f, t + (far ? 2.f : 1.f) / 17)),
                 .012f * (1 - .85f * t));
            if (i < 3)
                continue;
            for (float side : {-1.f, 1.f}) {
                float attach = t - side * .012f;
                V3 base = rachis(attach);
                float breadth = .46f * std::sin(pi * std::pow(attach, .66f));
                V3 tip = base + V3{-.09f, .036f, side * breadth};
                if (far) {
                    blade(base, (base + tip) * .5f, tip, {0, 1, 0}, .046f * std::sin(pi * attach));
                    continue;
                }
                for (int j = 0; j < 4; ++j) {
                    V3 pivot = base + (tip - base) * (j * .21f);
                    V3 span = (tip - base) * .22f;
                    float width = .028f * (1 - j * .15f);
                    for (float edge : {-1.f, 1.f}) {
                        V3 leaflet = pivot + span + V3{0, width * edge, 0};
                        blade(pivot, (pivot + leaflet) * .5f, leaflet, {0, 1, 0}, width * .48f);
                    }
                }
            }
        }
    } else if (mesh == MeshType::BellflowerStem) {
        const V3 bends[]{{0, -.5f, 0}, {-.026f, -.16f, 0}, {-.018f, .22f, 0},
                         {.043f, .43f, 0}, {.10f, .46f, 0}};
        for (int i = 0; i < 4; ++i)
            stem(bends[i], bends[i + 1], .016f - .002f * i);
        for (int i = 0; i < 7; ++i) {
            const float a = i * 2.39996f;
            V3 dir{std::cos(a), 0, std::sin(a)}, side{-dir.z, 0, dir.x};
            const bool basal = i < 4;
            V3 from{0, basal ? -.46f + .016f * i : -.15f + (i - 4) * .17f, 0};
            V3 tip = from + dir * (basal ? .35f : .15f) + V3{0, .065f, 0};
            if (basal) {
                V3 neck = from + dir * .13f + V3{0, .045f, 0};
                stem(from, neck, .006f);
                V3 middle = (neck + tip) * .5f + V3{0, .03f, 0};
                const V3 rim[]{neck, middle - dir * .05f + side * .078f,
                               middle + dir * .065f + side * .07f, tip,
                               middle + dir * .065f - side * .07f,
                               middle - dir * .05f - side * .078f};
                for (int edge = 0; edge < 6; ++edge)
                    triangle(middle, rim[edge], rim[(edge + 1) % 6], true);
            } else {
                blade(from, (from + tip) * .5f + V3{0, .03f, 0}, tip, side, .022f);
            }
        }
    } else if (mesh == MeshType::BellflowerBloom) {
        auto ring = [](int i, float h, float radius) {
            float a = i * (2 * pi / 20);
            return V3{std::cos(a) * radius, h, std::sin(a) * radius};
        };
        auto rim = [&](int i) {
            float lobe = std::cos(i * (2 * pi / 4));
            return ring(i, .32f + lobe * .16f, .365f + lobe * .10f);
        };
        for (int i = 0; i < 20; ++i) {
            V3 from = ring(i, -.5f, .13f), next = ring(i + 1, -.5f, .13f);
            V3 mid = ring(i, .05f, .31f), midNext = ring(i + 1, .05f, .31f);
            triangle({0, -.5f, 0}, from, next, true);
            triangle(from, mid, next, true);
            triangle(next, mid, midNext, true);
            triangle(mid, rim(i), midNext, true);
            triangle(midNext, rim(i), rim(i + 1), true);
        }
    }
}
} // namespace pl
