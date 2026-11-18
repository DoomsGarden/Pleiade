#pragma once
#include "vegetation_meshes.hpp"

namespace pl {
enum class TreeSpecies { Spruce, ScotsPine, Birch };
enum class GroundPlant { Juniper, Fern, Bellflower };

class VegetationRandom {
    uint32_t state;

  public:
    explicit VegetationRandom(uint32_t seed) : state(seed) {}
    float range(float low, float high) {
        state = state * 1664525u + 1013904223u;
        return low + (high - low) * (float(state >> 8) / 16777216.f);
    }
};

inline V3 horizontal(float angle) { return {std::cos(angle), 0, std::sin(angle)}; }

constexpr size_t noVegetationParent = size_t(-1);

// Mesh coordinates are centered, with the growing direction along local +Y.
inline size_t appendVegetationPart(std::vector<Object> &objects, MeshType mesh, V3 center, V3 axis,
                                   V3 size, V3 color, float pattern, VegetationSection section,
                                   size_t parent, int lodClass = 0, float roll = 0) {
    V3 y = unit(axis);
    V3 x = unit(cross(y, std::abs(y.z) < .92f ? V3{0, 0, 1} : V3{0, 1, 0}));
    V3 z = cross(x, y);
    const V3 rotated = x * std::cos(roll) + z * std::sin(roll);
    z = cross(rotated, y);
    x = rotated;
    x = x * size.x;
    y = y * size.y;
    z = z * size.z;
    M4 model = M4::identity();
    const V3 columns[]{x, y, z, center};
    for (int col = 0; col < 4; ++col) {
        model.at(0, col) = columns[col].x;
        model.at(1, col) = columns[col].y;
        model.at(2, col) = columns[col].z;
    }
    V3 half{std::abs(x.x) + std::abs(y.x) + std::abs(z.x),
            std::abs(x.y) + std::abs(y.y) + std::abs(z.y),
            std::abs(x.z) + std::abs(y.z) + std::abs(z.z)};
    const size_t index = objects.size();
    const uint16_t parentDistance = parent == noVegetationParent ? 0 : uint16_t(index - parent);
    objects.push_back({center, half * .53f, mesh,
                       {model, {color.x, color.y, color.z, 1}, {.85f, 0, pattern, 0}},
                       lodClass, section, parentDistance});
    return index;
}

inline size_t appendWood(std::vector<Object> &objects, V3 from, V3 to, float diameter, V3 color,
                         VegetationSection section, size_t parent, bool fine = false, bool tip = false,
                         SurfaceLayer material = SurfaceLayer::Bark) {
    const V3 axis = to - from;
    // Overlap the ends of neighboring sections at bends and grafts.
    const size_t index = appendVegetationPart(
        objects, tip ? MeshType::Branch : MeshType::TreeWood, (from + to) * .5f,
        axis, {diameter, length(axis) + diameter * .30f, diameter}, color, materialPattern(material),
        section, parent, fine ? 3 : 0);
    return index;
}

template <class HeightAt>
void appendTreeRoots(std::vector<Object> &objects, V3 base, float diameter, bool spruce,
                     float orientation, V3 bark, size_t collar, VegetationRandom &random,
                     HeightAt heightAt) {
    for (int i = 0; i < 5; ++i) {
        const bool longer = i >= 2;
        const float angle = orientation + i * (2 * pi / 5) + random.range(-.33f, .33f);
        const float reach = diameter * (longer ? random.range(1.65f, spruce ? 2.65f : 2.2f)
                                               : random.range(.85f, 1.25f));
        const float bend = longer ? random.range(.22f, .38f) * (i % 2 ? -1 : 1) : 0;
        const V3 direction = horizontal(angle), side = horizontal(angle + pi * .5f);
        const int segments = longer ? 4 : 2;
        auto point = [&](float t) {
            V3 p = base + direction * (.15f * diameter + reach * t) + side * (reach * bend * t * t);
            p.y = heightAt(p.x, p.z) + diameter * (.13f * (1 - t) - .33f * t * t * t);
            return p;
        };
        V3 previous = point(0);
        float width = diameter * random.range(.31f, .47f);
        size_t parent = collar, forkParent = collar;
        for (int segment = 1; segment <= segments; ++segment) {
            const V3 next = point(float(segment) / segments);
            parent = appendWood(objects, previous, next, width, bark, VegetationSection::Root,
                                parent, true, segment == segments);
            if (segment == segments / 2)
                forkParent = parent;
            previous = next;
            width *= .76f;
        }
        if (longer && (i == 2 || random.range(0, 1) > .50f)) {
            V3 fork = point(.5f);
            V3 offset = horizontal(angle + (i % 2 ? -.78f : .88f)) * (reach * .44f);
            V3 mid = fork + offset * .48f, tip = fork + offset;
            mid.y = heightAt(mid.x, mid.z) - diameter * .025f;
            tip.y = heightAt(tip.x, tip.z) - diameter * .21f;
            size_t forkSection = appendWood(objects, fork, mid, width * .95f, bark,
                                            VegetationSection::Root, forkParent, true);
            appendWood(objects, mid, tip, width * .72f, bark, VegetationSection::Root,
                       forkSection, true, true);
        }
    }
}

template <class HeightAt>
void appendTree(std::vector<Object> &objects, TreeSpecies species, V3 base, float height,
                uint32_t seed, HeightAt heightAt) {
    VegetationRandom random(seed);
    const bool spruce = species == TreeSpecies::Spruce, pine = species == TreeSpecies::ScotsPine;
    const float diameter = height * random.range(spruce ? .062f : .071f, spruce ? .078f : .094f);
    const float angle = random.range(0, 2 * pi);
    const float trunkWidth = diameter * random.range(1.19f, 1.33f);
    const V3 trunkX = horizontal(angle), trunkZ = cross(trunkX, {0, 1, 0});
    const V3 bark = pine ? V3{.93f, .88f, .84f} : spruce ? V3{.88f, .95f, .96f} : V3{.97f, .98f, .94f};
    const V3 upperBark = pine ? V3{1.40f, 1.04f, .77f} : bark;
    const V3 leaves = spruce ? V3{.82f, .92f, .83f} : pine ? V3{.77f, .90f, .90f} : V3{.97f, .96f, .82f};
    const auto canopy = spruce ? MeshType::SpruceNeedles : pine ? MeshType::PineNeedles : MeshType::BirchLeaves;
    const auto barkLayer = canopy == MeshType::BirchLeaves ? SurfaceLayer::BirchBark : SurfaceLayer::Bark;
    const float leafMaterial = foliageMaterialForMesh(canopy, materialPattern(SurfaceLayer::Broadleaf));
    auto trunk = [&](float fraction) {
        const V3 center = treeTrunkCenter(std::clamp((fraction + .04f) / 1.04f, 0.f, 1.f));
        return base + trunkX * (center.x * trunkWidth) + trunkZ * (center.z * trunkWidth) +
                      V3{0, height * fraction, 0};
    };
    auto taper = [diameter](float f) { return diameter * (1 - .86f * f); };
    const size_t collar = appendVegetationPart(objects, MeshType::TreeTrunk,
        base + V3{0, height * .48f, 0}, {0, 1, 0}, {trunkWidth, height * 1.04f, trunkWidth}, bark,
        materialPattern(barkLayer), VegetationSection::Collar, noVegetationParent, 0, angle);
    // Pine's upper bark is blended on the continuous mesh. Birch keeps its pale outer bark.
    objects[collar].gpu.surface.w = pine ? -3.f : spruce ? 0.f : -4.f;
    constexpr std::array levels{.12f, .38f, .63f, .83f, 1.f};
    std::array<size_t, levels.size() - 1> trunkSections{};
    size_t trunkParent = collar;
    for (size_t i = 0; i < trunkSections.size(); ++i) {
        trunkParent = appendWood(objects, trunk(levels[i]), trunk(levels[i + 1]), taper(levels[i]),
                                 i == 0 ? bark : upperBark, VegetationSection::Trunk, trunkParent,
                                 false, i == trunkSections.size() - 1);
        objects[trunkParent].visible = false;
        trunkSections[i] = trunkParent;
    }
    appendTreeRoots(objects, base, diameter, spruce, angle, bark, collar, random, heightAt);

    const int tiers = spruce ? 7 : pine ? 5 : 5;
    const int limbs = spruce ? 4 : pine ? 4 : 5;
    for (int tier = 0; tier < tiers; ++tier) {
        float level = spruce ? .26f + .103f * tier : pine ? .49f + .096f * tier : .35f + .126f * tier;
        const float span = height * (spruce ? .41f * (1 - level) : pine ? .30f - .017f * tier : .38f - .047f * tier);
        for (int limb = 0; limb < limbs; ++limb) {
            float heading = angle + tier * 2.39996f + limb * (2 * pi / limbs) + random.range(-.2f, .2f);
            float reach = span * random.range(.8f, 1.14f);
            float attach = level + random.range(-.027f, .027f);
            auto upper = std::upper_bound(levels.begin(), levels.end(), attach);
            size_t section = size_t(upper - levels.begin() - 1);
            V3 from = trunk(attach);
            V3 out = horizontal(heading) * reach, side = horizontal(heading + pi * .5f) * reach;
            const float rise = spruce ? .33f * level - .20f : .21f;
            V3 elbow = from + out * .53f + V3{0, reach * rise, 0};
            const V3 branchBark = bark * (1 - level) + upperBark * level;
            size_t primary = appendWood(objects, from, elbow, taper(level) * .25f, branchBark,
                                        VegetationSection::PrimaryBranch, trunkSections[section], true, false, barkLayer);
            for (float sign : {-1.f, 1.f}) {
                V3 fork = from + (elbow - from) * (sign < 0 ? .66f : .97f);
                V3 twig = fork + out * .27f + side * (sign * random.range(.14f, .27f)) +
                          V3{0, reach * (spruce ? -.045f : .17f), 0};
                size_t secondary = appendWood(objects, fork, twig, taper(level) * .16f, branchBark,
                                              VegetationSection::SecondaryBranch, primary, true, false, barkLayer);
                V3 growth = out * random.range(.25f, .39f) + side * (sign * .11f) +
                            V3{0, reach * (spruce ? .085f : .23f), 0};
                size_t shoot = appendWood(objects, twig, twig + growth, taper(level) * .083f,
                                          branchBark, VegetationSection::Shoot, secondary, true, true, barkLayer);
                const float breadth = reach * (canopy == MeshType::BirchLeaves ? .50f : .85f);
                appendVegetationPart(objects, canopy, twig + growth * .52f, growth,
                                     {breadth, length(growth) * 1.41f, breadth},
                                     leaves * random.range(.84f, 1.12f), leafMaterial,
                                     VegetationSection::Foliage, shoot, 0, random.range(-pi, pi));
                if (spruce || (pine && sign > 0)) {
                    const V3 older = twig - fork;
                    appendVegetationPart(objects, canopy, fork + older * .68f, older,
                                         {reach * .56f, length(older) * 1.07f, reach * .56f},
                                         leaves * .85f, leafMaterial, VegetationSection::Foliage, secondary,
                                         0, heading + sign * .5f);
                }
            }
        }
    }
    V3 leader = trunk(.90f), tip = trunk(1.02f);
    size_t shoot = appendWood(objects, leader, tip, taper(.95f) * .53f, upperBark,
                              VegetationSection::Shoot, trunkSections.back(), true, true, barkLayer);
    appendVegetationPart(objects, canopy, (leader + tip) * .5f, tip - leader,
                         {height * .11f, height * .19f, height * .11f}, leaves, leafMaterial,
                         VegetationSection::Foliage, shoot);
}

template <class HeightAt>
void appendGroundPlant(std::vector<Object> &objects, GroundPlant species, V3 base, float size,
                       uint32_t seed, HeightAt heightAt) {
    VegetationRandom random(seed);
    const float angle = random.range(0, 2 * pi);
    const V3 wood{.80f, .70f, .57f};
    const float collarWidth = size * (species == GroundPlant::Juniper ? .095f : .045f);
    const size_t collar = appendWood(objects, base - V3{0, size * .085f, 0},
                                     base + V3{0, size * .032f, 0}, collarWidth, wood,
                                     VegetationSection::Collar, noVegetationParent, true);
    if (species == GroundPlant::Juniper) {
        appendTreeRoots(objects, base, collarWidth, false, angle, wood, collar, random, heightAt);
        for (int i = 0; i < 7; ++i) {
            const V3 out = horizontal(angle + i * 2.39996f) * (size * random.range(.36f, .76f));
            V3 start = base + out * .035f;
            start.y = heightAt(start.x, start.z) - size * .025f;
            V3 elbow = base + out * .68f;
            elbow.y = heightAt(elbow.x, elbow.z) + size * random.range(.055f, .115f);
            V3 end = elbow + out * .27f + V3{0, size * random.range(.12f, .24f), 0};
            size_t branch = appendWood(objects, start, elbow, size * .057f, wood,
                                       VegetationSection::PrimaryBranch, collar, true);
            size_t middle = appendWood(objects, elbow, end, size * .043f, wood,
                                       VegetationSection::SecondaryBranch, branch, true);
            const V3 growth = out * .21f + V3{0, size * .08f, 0};
            size_t shoot = appendWood(objects, end, end + growth, size * .022f, wood,
                                      VegetationSection::Shoot, middle, true, true);
            const V3 green = V3{.81f, .88f, .87f} * random.range(.83f, 1.16f);
            const V3 axis = end - elbow;
            const V3 lower = elbow - start;
            appendVegetationPart(objects, MeshType::JuniperNeedles, start + lower * .76f, lower + V3{0, size * .19f, 0},
                                 {size * .31f, length(lower) * .59f, size * .37f}, green * .84f, materialPattern(SurfaceLayer::ConiferNeedle),
                                 VegetationSection::Foliage, branch, 3, angle + i * 2.4f);
            appendVegetationPart(objects, MeshType::JuniperNeedles, elbow + axis * .5f, axis,
                                 {size * .48f, length(axis) * 1.13f, size * .46f}, green, materialPattern(SurfaceLayer::ConiferNeedle),
                                 VegetationSection::Foliage, middle, 3);
            appendVegetationPart(objects, MeshType::JuniperNeedles, end + growth * .5f, growth,
                                 {size * .36f, length(growth), size * .32f}, green * 1.10f, materialPattern(SurfaceLayer::ConiferNeedle),
                                 VegetationSection::Foliage, shoot, 3);
        }
        return;
    }

    V3 below = base - V3{0, size * .075f, 0};
    for (int i = 0; i < 3; ++i) {
        V3 end = below + horizontal(angle + i * 2.39996f) * (size * random.range(.055f, .15f));
        end.y = heightAt(end.x, end.z) - size * random.range(.13f, .21f);
        appendWood(objects, below, end, collarWidth * .42f, wood, VegetationSection::Root,
                   collar, true, true);
    }
    if (species == GroundPlant::Fern) {
        for (int i = 0; i < 8; ++i) {
            V3 direction = horizontal(angle + i * 2.39996f) * random.range(.53f, 1.1f) +
                           V3{0, random.range(.43f, .97f), 0};
            float span = size * random.range(.72f, 1.17f);
            appendVegetationPart(objects, MeshType::FernFrond, base + unit(direction) * (span * .49f),
                                 direction, {span * .62f, span, span * .62f},
                                 V3{.87f, .98f, .81f} * random.range(.79f, 1.15f), materialPattern(SurfaceLayer::FernPinna),
                                 VegetationSection::Foliage, collar, 3, angle + i * 2.39996f);
        }
    } else {
        for (int stalk = 0; stalk < 4; ++stalk) {
            const V3 out = horizontal(angle + stalk * 2.39996f);
            V3 start = base + out * (size * .02f);
            start.y = heightAt(start.x, start.z);
            float height = size * random.range(.66f, 1.08f);
            V3 axis = unit(out * .19f + V3{0, 1, 0});
            const size_t stem = appendVegetationPart(
                objects, MeshType::BellflowerStem, start + axis * (height * .5f),
                axis, {height, height, height}, {.78f, .89f, .72f}, materialPattern(SurfaceLayer::Broadleaf),
                VegetationSection::Shoot, collar, 3);
            // The stem mesh ends at +.10 along local X and +.46 along local Y.
            V3 hook = start + axis * (height * .96f) + unit(cross(axis, {0, 0, 1})) * (height * .10f);
            V3 head = unit(V3{.3f, -1, 0} + out * .35f);
            const float bell = height * random.range(.14f, .20f);
            appendVegetationPart(objects, MeshType::BellflowerBloom, hook + head * (bell * .47f), head,
                                 {bell * .92f, bell, bell * .92f},
                                 V3{.86f, .87f, 1.f} * random.range(.88f, 1.16f), materialPattern(SurfaceLayer::Petal),
                                 VegetationSection::Flower, stem, 3);
        }
    }
}
} // namespace pl
