#pragma once
#include "scene.hpp"

namespace pl {
inline constexpr std::array vegetationMeshes{
    MeshType::TreeWood, MeshType::SpruceNeedles, MeshType::PineNeedles, MeshType::JuniperNeedles,
    MeshType::FernFrond, MeshType::BellflowerStem, MeshType::BellflowerBloom,
    MeshType::SpruceNeedlesFar, MeshType::PineNeedlesFar, MeshType::JuniperNeedlesFar,
    MeshType::FernFrondFar, MeshType::TreeTrunk, MeshType::BirchLeaves, MeshType::BirchLeavesFar,
    MeshType::BambooStem, MeshType::BambooLeaves, MeshType::PalmFrond, MeshType::Branch,
    MeshType::TreeTrunkFar, MeshType::BambooStemFar, MeshType::BambooLeavesFar, MeshType::PalmFrondFar,
    MeshType::SpruceNeedlesMid, MeshType::PineNeedlesMid, MeshType::JuniperNeedlesMid, MeshType::VineStem,
    MeshType::BirchLeavesMid,MeshType::BambooLeavesMid,MeshType::PalmFrondMid,MeshType::FernFrondMid};

inline MeshType vegetationMiddleMesh(MeshType mesh) {
    switch (mesh) {
    case MeshType::SpruceNeedles: return MeshType::SpruceNeedlesMid;
    case MeshType::PineNeedles: return MeshType::PineNeedlesMid;
    case MeshType::JuniperNeedles: return MeshType::JuniperNeedlesMid;
    case MeshType::BirchLeaves: return MeshType::BirchLeavesMid;
    case MeshType::BambooLeaves: return MeshType::BambooLeavesMid;
    case MeshType::PalmFrond: return MeshType::PalmFrondMid;
    case MeshType::FernFrond: return MeshType::FernFrondMid;
    default: return mesh;
    }
}

inline MeshType vegetationFarMesh(MeshType mesh) {
    switch (mesh) {
    case MeshType::SpruceNeedles: return MeshType::SpruceNeedlesFar;
    case MeshType::PineNeedles: return MeshType::PineNeedlesFar;
    case MeshType::JuniperNeedles: return MeshType::JuniperNeedlesFar;
    case MeshType::FernFrond: return MeshType::FernFrondFar;
    case MeshType::BirchLeaves: return MeshType::BirchLeavesFar;
    case MeshType::TreeTrunk: return MeshType::TreeTrunkFar;
    case MeshType::BambooStem: return MeshType::BambooStemFar;
    case MeshType::BambooLeaves: return MeshType::BambooLeavesFar;
    case MeshType::PalmFrond: return MeshType::PalmFrondFar;
    default: return mesh;
    }
}

inline float foliageMaterialForMesh(MeshType mesh, float fallback) {
    switch (mesh) {
    case MeshType::PineNeedles: return materialPattern(SurfaceLayer::PineNeedle);
    case MeshType::PineNeedlesMid: return materialPattern(SurfaceLayer::PineNeedle);
    case MeshType::SpruceNeedles:
    case MeshType::SpruceNeedlesMid:
    case MeshType::JuniperNeedlesMid:
    case MeshType::JuniperNeedles: return materialPattern(SurfaceLayer::ConiferNeedle);
    case MeshType::PineNeedlesFar: return materialPattern(SurfaceLayer::PineSpray);
    case MeshType::SpruceNeedlesFar:
    case MeshType::JuniperNeedlesFar: return materialPattern(SurfaceLayer::ConiferSpray);
    case MeshType::BirchLeavesFar: return materialPattern(SurfaceLayer::BroadleafSpray);
    case MeshType::FernFrondFar: return materialPattern(SurfaceLayer::FernSpray);
    default: return fallback;
    }
}

// The same curve is used to place the branches and to build the trunk surface.
inline V3 treeTrunkCenter(float t) {
    return {t * t * .18f + .023f * std::sin(pi * t * 1.8f) * t,
            t - .5f, -.051f * std::sin(pi * t) * t};
}

template <class Curve, class Radius, class Triangle>
void vegetationTube(Curve center, Radius radius, int rings, int sides, bool caps, Triangle triangle) {
    auto point = [&](int ring, int sector) {
        const float t = float(ring) / rings, azimuth = float(sector) / sides * (2 * pi);
        const V3 d = unit(center(std::min(t + .006f, 1.f)) - center(std::max(0.f, t - .006f)));
        const V3 right = unit(cross(d, std::abs(d.z) < .90f ? V3{0, 0, 1} : V3{1, 0, 0}));
        const V3 across = cross(right, d);
        const V3 radial = right * std::cos(azimuth) + across * std::sin(azimuth);
        return Vertex{center(t) + radial * radius(t, azimuth), radial, float(sector) / sides, t};
    };
    for (int row = 0; row < rings; ++row)
        for (int col = 0; col < sides; ++col) {
            const auto a = point(row, col), b = point(row, col + 1);
            const auto c = point(row + 1, col), d = point(row + 1, col + 1);
            triangle(a, c, b, false);
            triangle(b, c, d, false);
        }
    if (!caps)
        return;
    for (int col = 0; col < sides; ++col) {
        auto a = point(0, col), b = point(0, col + 1);
        auto c = point(rings, col), d = point(rings, col + 1);
        a.n = b.n = unit(center(0) - center(.01f));
        c.n = d.n = unit(center(1) - center(.99f));
        triangle({center(0), a.n, .5f, 0.f}, a, b, false);
        triangle({center(1), c.n, .5f, 1.f}, d, c, false);
    }
}

template <class Triangle>
void buildVegetationMesh(MeshType mesh, Triangle triangle) {
    auto ribbon = [&](V3 from, V3 tip, V3 across, float width, float arch, int steps, bool tapered, bool centralUV = false) {
        const V3 length = tip - from;
        const V3 baseNormal = unit(cross(across, length));
        auto point = [&](int row, int col) {
            const float t = float(row) / steps, edge = float(col - 1);
            const float curve = std::sin(pi * t);
            const float profile = tapered ? .27f + (1 - t) * .67f + .14f * curve : 1.f;
            const V3 bend = baseNormal * (arch * curve);
            const V3 normal = unit(baseNormal - unit(length) * (arch * pi * std::cos(pi * t) /
                                   std::max(.001f, pl::length(length))) - across * (edge * .23f * curve));
            const V3 p = from + length * t + bend + across * (width * profile * edge) -
                         baseNormal * (std::abs(edge) * width * .17f * curve);
            return Vertex{p, normal, centralUV ? .42f + .08f * col : float(col) * .5f,
                          centralUV ? .22f + .54f * t : t};
        };
        for (int row = 0; row < steps; ++row)
            for (int col = 0; col < 2; ++col) {
                const auto a = point(row, col), b = point(row, col + 1);
                const auto c = point(row + 1, col), d = point(row + 1, col + 1);
                triangle(a, b, c, true);
                triangle(b, d, c, true);
            }
    };
    auto stalk = [&](V3 from, V3 to, float width) {
        const V3 forward = unit(to - from);
        const V3 x = unit(cross(forward, std::abs(forward.z) < .90f ? V3{0, 0, 1} : V3{0, 1, 0}));
        ribbon(from, to, x, width, 0.f, 1, true, true);
        ribbon(from, to, cross(x, forward), width, 0.f, 1, true, true);
    };
    const V3 up{0, 1, 0};
    if (mesh == MeshType::VineStem) {
        vegetationTube([](float t) { return V3{0, t - .5f, 0}; },
                       [](float, float) { return .50f; }, 1, 6, false, triangle);
        return;
    }
    const bool trunk = mesh == MeshType::TreeTrunk || mesh == MeshType::TreeTrunkFar;
    const bool bambooStem = mesh == MeshType::BambooStem || mesh == MeshType::BambooStemFar;
    if (mesh == MeshType::TreeWood || trunk || bambooStem || mesh == MeshType::Branch) {
        auto center = [&](float t) {
            const float curve=std::sin(pi*t);
            return trunk?treeTrunkCenter(t):V3{bambooStem?0.f:.058f*curve*curve,t-.5f,0};
        };
        auto radius = [&](float t, float angle) {
            if (trunk) {
                const float flare = .090f * std::exp(-t * 35);
                const float twist = .018f * (std::sin(3 * angle + 5 * t) + .40f * std::sin(7 * angle - 3 * t));
                return .35f * std::pow(std::max(.028f, 1 - t), .68f) + flare + twist * (1 - .83f * t);
            }
            if (bambooStem)
                return .46f - .16f * t + .028f * std::pow(.5f + .5f * std::cos(2 * pi * 12 * t), 5.f);
            const float taper=mesh==MeshType::Branch?.455f:.188f;
            return .476f-taper*t+.036f*std::exp(-t*14)+.008f*std::sin(5*angle+3*t)*(1-t);
        };
        const int rings = mesh == MeshType::TreeTrunk ? 26 : mesh == MeshType::TreeTrunkFar ? 9 :
                          mesh == MeshType::BambooStem ? 48 : mesh == MeshType::BambooStemFar ? 4 : 4;
        const int sides = mesh == MeshType::TreeTrunk ? 24 : mesh == MeshType::BambooStem ? 10 : 8;
        vegetationTube(center, radius, rings, sides, trunk||bambooStem, triangle);
        return;
    }

    if (mesh == MeshType::PineNeedlesFar || mesh == MeshType::SpruceNeedlesFar ||
        mesh == MeshType::JuniperNeedlesFar || mesh == MeshType::BirchLeavesFar) {
        const float breadth = mesh == MeshType::PineNeedlesFar ? .44f : .46f;
        ribbon({0, -.5f, 0}, {0, .49f, 0}, {1, 0, 0}, breadth, .045f, 2, false);
        ribbon({0, -.5f, 0}, {0, .48f, 0}, {0, 0, 1}, breadth, -.033f, 2, false);
        return;
    }
    const bool pine = mesh == MeshType::PineNeedles || mesh == MeshType::PineNeedlesMid;
    const bool juniper = mesh == MeshType::JuniperNeedles || mesh == MeshType::JuniperNeedlesMid;
    const bool spruce = mesh == MeshType::SpruceNeedles || mesh == MeshType::SpruceNeedlesMid;
    if (pine || juniper || spruce) {
        const bool mid = mesh == MeshType::PineNeedlesMid || mesh == MeshType::JuniperNeedlesMid || mesh == MeshType::SpruceNeedlesMid;
        stalk({0, -.49f, 0}, {0, .41f, 0}, mid ? .006f : .008f);
        auto needle = [&](V3 from, V3 tip, V3 across, float width, float curve) {
            const V3 n = unit(cross(across, tip - from));
            if (!mid) {
                ribbon(from, tip, across, width, curve, pine ? 3 : 2, true);
                return;
            }
            // A narrow bent strip with a pointed end stays distinct at screen scale.
            const V3 middle = (from + tip) * .5f + n * curve;
            const Vertex a{from - across * width * .38f, n, 0, 0};
            const Vertex b{from + across * width * .38f, n, 1, 0};
            const Vertex c{middle - across * width, n, 0, .5f};
            const Vertex d{middle + across * width, n, 1, .5f};
            triangle(a, b, c, true);
            triangle(b, d, c, true);
            triangle(c, d, {tip, n, .5f, 1}, true);
        };
        if(pine){
            // Pine fascicles are concentrated on several distinct terminal shoots.
            const int shoots=mid?5:8, pairs=mid?13:25;
            for(int i=0;i<shoots;++i){
                const float t=float(i)/shoots, a=i*2.39996f+.61f*std::sin(i*4.1f);
                const V3 outward{std::cos(a),0,std::sin(a)}, across{-outward.z,0,outward.x};
                const V3 start{0,-.40f+.60f*t,0};
                const V3 growth=outward*(.23f-.072f*t)+up*(.16f+.04f*std::cos(i*2.8f));
                stalk(start,start+growth,.0037f);
                for(int j=0;j<pairs;++j){
                    const float f=float(j)/pairs, angle=j*2.39996f;
                    const V3 origin=start+growth*(.48f+.54f*f)+across*(.009f*std::sin(j*5.f));
                    const V3 spread=across*std::cos(angle)+up*(.53f*std::sin(angle));
                    for(float side : {-1.f,1.f}){
                        const V3 tip=origin+unit(growth)*(.075f+.037f*std::sin(j*3.3f+i))+spread*.064f+across*(side*.014f);
                        needle(origin,tip,unit(cross(tip-origin,up)),mid?.0045f:.0036f,.010f*side);
                    }
                }
            }
            return;
        }
        const int twigs = mid ? 9 : 13;
        for (int i = 0; i < twigs; ++i) {
            const float t = float(i) / twigs, a = (i%2?0.f:pi)+.39f*std::sin(i*2.6f)+(juniper?.36f*std::cos(i*4.f):0);
            const V3 dir{std::cos(a), 0, std::sin(a)}, side{-dir.z, 0, dir.x};
            const V3 start{.018f * std::sin(8 * t), -.43f + .77f * t + .021f * std::sin(i * 5.6f), 0};
            const V3 twig = dir * (.325f * (1 - .54f * t) * (1 + .15f * std::sin(i * 3.8f))) + up * (.087f+.037f*std::cos(i*3.f));
            stalk(start, start + twig, .004f);
            for (int j = 0; j < (mid ? 8 : 12); ++j) {
                const V3 from = start + twig * (.18f + .70f * j/(mid?8.f:12.f) + .03f * std::sin(i * 7.f + j * 5.f));
                    for (int leaf = 0; leaf < (juniper ? 3 : 2); ++leaf) {
                        const float roll = (leaf * (juniper ? 2.0944f : 3.1416f)) + j * .63f + i * .45f;
                        const V3 spread = side * std::cos(roll) + up * std::sin(roll);
                        const V3 to = from + dir * .043f + spread * ((juniper ? .058f : .071f) * (1 + .24f * std::sin(i * 5.f + j)));
                        needle(from, to, unit(cross(to - from, dir)), juniper ? .0083f : .0061f, .007f);
                    }
            }
        }
        return;
    }

    if (mesh == MeshType::FernFrond || mesh == MeshType::FernFrondFar || mesh==MeshType::FernFrondMid) {
        const bool farDetail = mesh == MeshType::FernFrondFar;
        const bool mid = mesh == MeshType::FernFrondMid;
        if (farDetail) {
            ribbon({0,-.49f,0},{-.04f,.49f,0},{0,0,1},.49f,-.201f,4,false);
            return;
        }
        auto rachis = [](float t) { return V3{.23f * std::sin(pi * t) - .04f * t, t - .5f,
                                              .026f * std::sin(pi * t * 2)}; };
        const int pinnae = mid?11:18;
        for (int i = 0; i < pinnae; ++i) {
            const float t = float(i) / pinnae;
            stalk(rachis(t), rachis(t + 1.f / pinnae), .013f * (1 - .86f * t));
            if (t < .14f) continue;
            for (float side : {-1.f, 1.f}) {
                const float attach = t + side * .013f + .008f * std::sin(i * 3.9f);
                const V3 from = rachis(attach);
                const float breadth = .395f * std::pow(std::sin(pi * std::pow(attach, .77f)), 1.2f);
                const V3 tip = from + V3{-.056f + .019f * std::sin(i * 2.f),
                    std::min(.476f - from.y, .033f + .055f * attach), side * breadth};
                ribbon(from, tip, unit(cross(tip - from, {1, 0, 0})),
                       std::min(.048f, .29f * breadth), .035f * (1 - attach), mid?1:3, false);
            }
        }
        return;
    }

    const bool bambooLeaf = mesh==MeshType::BambooLeaves||mesh==MeshType::BambooLeavesFar||mesh==MeshType::BambooLeavesMid;
    if (mesh==MeshType::BirchLeaves||mesh==MeshType::BirchLeavesMid||bambooLeaf) {
        const bool farDetail = mesh == MeshType::BambooLeavesFar;
        const bool mid = mesh == MeshType::BirchLeavesMid || mesh == MeshType::BambooLeavesMid;
        stalk({0, -.5f, 0}, {.020f, .46f, 0}, .013f);
        const int count=bambooLeaf?(farDetail?7:mid?11:16):(mid?22:31);
        for (int i = 0; i < count; ++i) {
            const float t = float(i) / count, a = i * 2.39996f;
            const V3 dir{std::cos(a), 0, std::sin(a)}, crosswise{-dir.z, 0, dir.x};
            const V3 base{.020f * t, -.41f + .77f * t, 0};
            const V3 neck = base + dir * (bambooLeaf?.061f:.032f+.074f*std::sin(pi*t)) + up * .023f;
            stalk(base, neck, .005f);
            const float length=(bambooLeaf?.30f:.124f)*(1-.32f*t)*(1+.27f*std::sin(i*4.25f));
            const V3 tip=neck+dir*length+up*(.013f+.04f*std::cos(7*t));
            const V3 axis = unit(cross(crosswise, tip - neck));
            ribbon(neck,tip,unit(crosswise+axis*.40f),bambooLeaf?.023f:.042f,
                   bambooLeaf?.039f:.027f,farDetail||mid?1:3,false);
        }
        return;
    }

    if (mesh==MeshType::PalmFrond||mesh==MeshType::PalmFrondFar||mesh==MeshType::PalmFrondMid) {
        auto rachis = [](float t) { return V3{.22f * std::sin(pi * t) - .34f * t * t, t - .5f, 0}; };
        const bool farDetail = mesh == MeshType::PalmFrondFar;
        const bool mid = mesh == MeshType::PalmFrondMid;
        const int segments=farDetail?10:mid?17:25;
        for (int i = 0; i < segments; ++i) {
            const float t = float(i) / segments;
            stalk(rachis(t), rachis(t + 1.f / segments), .017f * (1 - .90f * t));
            if (t < .125f) continue;
            for (float side : {-1.f, 1.f}) {
                const V3 from = rachis(t);
                const float reach = .41f * std::pow(std::sin(pi * t), .75f) * (1 + .075f * std::sin(i * 3.4f + side));
                const V3 to = from + V3{-.065f - .16f * reach, .018f + .046f * (1 - t), side * reach};
                ribbon(from, to, unit(cross(to-from, {1,0,0})), reach*(farDetail?.098f:mid?.066f:.055f),
                       .019f, farDetail||mid?1:2, false);
            }
        }
        return;
    }

    if (mesh == MeshType::BellflowerStem) {
        const V3 bends[]{{0, -.5f, 0}, {-.026f, -.16f, 0}, {-.018f, .22f, 0},
                         {.043f, .43f, 0}, {.10f, .46f, 0}};
        for (int i = 0; i < 4; ++i) stalk(bends[i], bends[i + 1], .016f - .002f * i);
        for (int i = 0; i < 7; ++i) {
            const float a = i * 2.39996f;
            const V3 dir{std::cos(a), 0, std::sin(a)}, side{-dir.z, 0, dir.x};
            const bool basal = i < 4;
            const V3 from{0, basal ? -.46f + .018f * i : -.15f + (i - 4) * .17f, 0};
            const V3 neck = from + dir * .07f + up * .04f;
            stalk(from, neck, .006f);
            ribbon(neck, neck + dir * (basal ? .27f : .13f) + up * .042f, side,
                   basal ? .095f : .028f, .035f, 3, false);
        }
        return;
    }
    if (mesh == MeshType::BellflowerBloom) {
        auto center = [](float t) { return V3{0, t - .5f, 0}; };
        auto radius = [](float t, float angle) {
            const float lobe = .5f + .5f * std::cos(5 * angle);
            return .119f + .205f * t + .092f * std::pow(t, 6) * lobe;
        };
        auto curlPetals = [&](Vertex a, Vertex b, Vertex c, bool) {
            auto curl = [](Vertex v) {
                const float lobe = .5f + .5f * std::cos(v.u * 10 * pi);
                v.p.y -= .15f * std::pow(v.v, 8) * (1 - lobe);
                return v;
            };
            triangle(curl(a), curl(b), curl(c), true);
        };
        vegetationTube(center, radius, 9, 40, false, curlPetals);
    }
}
} // namespace pl
