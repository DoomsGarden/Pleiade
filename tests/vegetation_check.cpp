#include "../tools/desktop-source/shore.hpp"
#include "../tools/desktop-source/vegetation_meshes.hpp"
#include "../tools/desktop-source/world.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using namespace pl;

static void checkHierarchy(const std::vector<Object> &objects) {
    size_t collar = 0;
    for (size_t index = 0; index < objects.size(); ++index) {
        const auto &part = objects[index];
        if (part.vegetation == VegetationSection::None)
            continue;
        for (float v : part.gpu.model.v)
            assert(std::isfinite(v));
        if (part.vegetation == VegetationSection::Collar) {
            assert(part.parentDistance == 0);
            collar = index;
            continue;
        }
        assert(part.parentDistance != 0 && part.parentDistance <= index - collar);
        const auto parent = objects[index - part.parentDistance].vegetation;
        switch (part.vegetation) {
        case VegetationSection::Root:
            assert(parent == VegetationSection::Collar || parent == VegetationSection::Root);
            break;
        case VegetationSection::Trunk:
            assert(parent == VegetationSection::Collar || parent == VegetationSection::Trunk);
            break;
        case VegetationSection::PrimaryBranch:
            assert(parent == VegetationSection::Collar || parent == VegetationSection::Trunk);
            break;
        case VegetationSection::SecondaryBranch:
            assert(parent == VegetationSection::PrimaryBranch);
            break;
        case VegetationSection::Shoot:
            assert(parent == VegetationSection::Collar || parent == VegetationSection::Trunk ||
                   parent == VegetationSection::SecondaryBranch);
            break;
        case VegetationSection::Foliage:
            assert(parent == VegetationSection::Collar || parent == VegetationSection::SecondaryBranch ||
                   parent == VegetationSection::Shoot);
            break;
        case VegetationSection::Flower:
            assert(parent == VegetationSection::Shoot);
            break;
        default: assert(false);
        }
    }
}

static V3 rootEnd(const Object &part, float sign) {
    const auto &m = part.gpu.model;
    V3 axis{m.at(0, 1), m.at(1, 1), m.at(2, 1)};
    V3 width{m.at(0, 0), m.at(1, 0), m.at(2, 0)};
    return part.center + (axis * .5f - unit(axis) * (length(width) * .15f)) * sign;
}

int main() {
    for (const MeshType mesh : vegetationMeshes) {
        size_t triangles = 0;
        buildVegetationMesh(mesh, [&](V3 a, V3 b, V3 c, bool) {
            assert(length(cross(b - a, c - a)) > 1e-8f);
            for (V3 p : {a, b, c})
                assert(std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) <= .501f);
            ++triangles;
        });
        assert(triangles > 0);
    }
    checkHierarchy(makeShoreScene().objects);
    const Chunk plain = generateChunk({0, -1}, 12345);
    const Chunk detailed = generateChunk({0, -1}, 12345, true);
    assert(plain.trees == detailed.trees && plain.grass == detailed.grass);
    checkHierarchy(detailed.objects);

    const V3 base{4, shoreHeight(4, -3), -3};
    for (TreeSpecies species : {TreeSpecies::Spruce, TreeSpecies::ScotsPine}) {
        std::vector<Object> tree, repeat;
        appendTree(tree, species, base, 6, 47, shoreHeight);
        appendTree(repeat, species, base, 6, 47, shoreHeight);
        checkHierarchy(tree);
        assert(tree.size() == repeat.size());
        bool hasFork = false;
        int rootsFromCollar = 0;
        std::vector<int> rootChildren(tree.size());
        for (size_t i = 0; i < tree.size(); ++i) {
            assert(tree[i].gpu.model.v == repeat[i].gpu.model.v);
            if (tree[i].vegetation != VegetationSection::Root)
                continue;
            size_t parent = i - tree[i].parentDistance;
            if (tree[parent].vegetation == VegetationSection::Collar) {
                ++rootsFromCollar;
            } else {
                assert(length(rootEnd(tree[i], -1) - rootEnd(tree[parent], 1)) < .001f);
                hasFork |= ++rootChildren[parent] > 1;
            }
        }
        assert(rootsFromCollar == 5 && hasFork);
        for (const auto &root : tree) {
            if (root.vegetation == VegetationSection::Root && root.mesh == MeshType::Branch) {
                const V3 end = rootEnd(root, 1);
                assert(end.y < shoreHeight(end.x, end.z));
            }
        }
    }
    std::cout << "PASS: finite vegetation meshes, reproducible hierarchy, terrain contact, root forks\n";
}
