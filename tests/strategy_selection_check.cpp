#include "../tools/desktop-source/strategy_selection.hpp"
#include "../tools/desktop-source/scout_group.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <limits>

using namespace pl;

static void checkPicking() {
    Camera camera;
    camera.strategy = true;
    camera.target = {14, 0, -8};
    constexpr float aspect = 16.f / 9;
    const auto flat = [](float, float) { return 0.f; };
    const auto noGround = [](float, float) { return std::numeric_limits<float>::quiet_NaN(); };
    const auto center = pickStrategy(camera, 0, 0, aspect, {}, flat);
    assert(center && !center->entity && length(center->point - camera.target) < .001f);
    assert(!pickStrategy(camera, 0, 0, aspect, {}, flat, center->distance - 1.f));
    assert(!strategyCursorRay(camera, 0, 0, 0));
    assert(!strategyCursorRay(camera, 0, -1.1f, aspect));

    camera.orbit = 1.1f;
    const V3 sample = camera.target + V3{3, 0, -2};
    const V4 clip = camera.projection(aspect) * camera.view() * V4{sample.x, sample.y, sample.z, 1};
    const auto cursor = pickStrategy(camera, clip.x / clip.w, clip.y / clip.w, aspect, {}, flat);
    assert(cursor && length(cursor->point - sample) < .001f);

    const auto ray = *strategyCursorRay(camera, 0, 0, aspect);
    const V3 front = ray.origin + ray.direction * 10.f;
    const V3 back = ray.origin + ray.direction * 17.f;
    const V3 hidden = ray.origin + ray.direction * (center->distance + 6.f);
    const V3 cropped = ray.origin + ray.direction * .03f;
    const V3 bounds{.5f, .5f, .5f};
    const std::vector<StrategyPickTarget> ordered{
        {81, back, bounds, 100}, {6, front, bounds, 4}, {2, front, bounds, 4},
        {3, front, bounds, 0}, {1, cropped, {.01f, .01f, .01f}, 100}};
    auto entity = pickStrategy(camera, 0, 0, aspect, ordered, flat);
    assert(entity && entity->entity == 2 && entity->distance < 10);
    const std::vector<StrategyPickTarget> farOnly{{42, hidden, bounds}};
    const auto obscured = pickStrategy(camera, 0, 0, aspect, farOnly, flat);
    assert(obscured && !obscured->entity);
    const auto unobscured = pickStrategy(camera, 0, 0, aspect, farOnly, noGround);
    assert(unobscured && unobscured->entity == 42);
    assert(!pickStrategy(camera, 0, 0, aspect, {}, noGround));

    const auto sloping = [](float x, float z) { return .21f * x - .14f * z; };
    const auto hill = pickStrategy(camera, .2f, -.4f, aspect, {}, sloping);
    assert(hill && std::abs(hill->point.y - sloping(hill->point.x, hill->point.z)) < .001f);
    camera.distance = 100;
    const auto distant = pickStrategy(camera, 0, .75f, aspect, {}, flat);
    assert(distant && distant->distance > 200.f);
    assert(!pickStrategy(camera, 0, .75f, aspect, {}, flat, 200.f));
    camera.strategy = false;
    assert(!pickStrategy(camera, 0, 0, aspect, {}, flat));
}

static void checkScoutOrder() {
    VisibilityMap map;
    Scout patrol;
    patrol.advance(map, .1);
    assert(patrol.initialized && !patrol.commanded && !patrol.path.empty() && patrol.goal == 1);

    Scout scout;
    const V3 target{20.7f, 0, 8.3f};
    const int wallColumn = map.index(14, 0) % VisibilityMap::size;
    for (int z = 0; z < VisibilityMap::size; ++z) {
        const int i = z * VisibilityMap::size + wallColumn;
        if (map.position(i).z != -1.f)
            map.obstacle[i] += 3.f;
    }
    assert(scout.command(map, target));
    assert(scout.initialized && scout.commanded && !scout.path.empty());
    const auto route = scout.path;
    for (int i : route)
        assert(map.walkable(i));
    assert(std::any_of(route.begin(), route.end(), [&](int i) {
        const auto p = map.position(i);
        return i % VisibilityMap::size == wallColumn && p.z == -1.f;
    }));
    scout.advance(map, .1);
    const auto step = scout.step;
    assert(!scout.command(map, {128, 0, 12}));
    assert(!scout.command(map, {14.5f, 0, 5.f}));
    assert(!scout.command(map, {std::numeric_limits<float>::quiet_NaN(), 0, 5.f}));
    assert(scout.path == route && scout.step == step);
    const int gap = map.index(14, -1);
    map.obstacle[gap] += 3.f;
    assert(!scout.command(map, target));
    assert(scout.path == route && scout.step == step);
    map.obstacle[gap] -= 3.f;

    for (int frame = 0; frame < 200; ++frame)
        scout.advance(map, .1);
    assert(scout.position.x == target.x && scout.position.z == target.z);
    const V3 stopped = scout.position;
    for (int frame = 0; frame < 200; ++frame)
        scout.advance(map, .1);
    assert(length(scout.position - stopped) == 0 && scout.goal == 0);
    assert(map.index(std::nextafter(128.f, 0.f), 127.f) == VisibilityMap::size * VisibilityMap::size - 1);
}

static void checkGroupOrders() {
    VisibilityMap map;
    ScoutGroup group;
    for (auto &scout : group.scouts) {
        const V3 start = scout.position;
        scout.advance(map, .1);
        assert(scout.path.empty() && length(scout.position - start) == 0);
    }
    group.select(1, false);
    group.select(2, true);
    group.select(2, true);
    assert(group.selectionCount() == 2 && !group.selected[0]);
    group.select(0, false);
    assert(group.selectionCount() == 1 && group.selected[0]);
    assert(group.command(map, {11.3f, 0, 5.7f}));
    assert(group.scouts[0].commanded && !group.scouts[1].commanded && !group.scouts[2].commanded);

    group.select(1, true);
    group.select(2, true);
    const V3 destination{26.3f, 0, 5.9f};
    for (float dz : {-2.f, 2.f})
        map.obstacle[map.index(destination.x, destination.z + dz)] += 3;
    assert(group.command(map, destination));
    const auto accepted = group.scouts;
    assert(!group.command(map, {128, 0, 5}));
    assert(!group.command(map, {destination.x, 0, destination.z + 2}));
    const int divider = map.index(0, 0) % VisibilityMap::size;
    for (int z = 0; z < VisibilityMap::size; ++z)
        map.obstacle[z * VisibilityMap::size + divider] += 3;
    assert(!group.command(map, {-15, 0, 5}));
    for (size_t i = 0; i < ScoutGroup::size; ++i) {
        assert(group.scouts[i].path == accepted[i].path && group.scouts[i].step == accepted[i].step);
    }

    for (int frame = 0; frame < 150; ++frame)
        for (auto &scout : group.scouts)
            scout.advance(map, .1);
    std::array<int, ScoutGroup::size> cells{};
    for (size_t i = 0; i < ScoutGroup::size; ++i) {
        auto &scout = group.scouts[i];
        const V3 stopped = scout.position;
        const int cell = map.index(stopped.x, stopped.z);
        cells[i] = cell;
        assert(map.walkable(cell));
        assert(length(stopped - destination) < 5);
        scout.advance(map, .1);
        assert(length(scout.position - stopped) == 0);
    }
    std::sort(cells.begin(), cells.end());
    assert(std::adjacent_find(cells.begin(), cells.end()) == cells.end());

    Scene a(0, false), b(0, false);
    group.scouts[0].append(a, 1);
    group.scouts[0].append(b, 2);
    assert(!a.objects.empty() && a.objects.size() == b.objects.size());
    for (size_t i = 0; i < a.objects.size(); ++i)
        assert(a.objects[i].gpu.model.v == b.objects[i].gpu.model.v);
}

static void checkGroupVisibility() {
    const std::array<V3, 2> observers{{{1, terrainHeight(1, 1), 1}, {11, terrainHeight(11, 1), 1}}};
    VisibilityMap first, second, shared;
    first.reveal(observers[0], 8);
    second.reveal(observers[1], 8);
    shared.reveal(observers, 8);
    assert(shared.revision == 1 && shared.rays < first.rays + second.rays);
    assert(shared.visibleCount > std::max(first.visibleCount, second.visibleCount));
    for (size_t i = 0; i < shared.pixels.size(); ++i)
        assert(shared.pixels[i] == std::max(first.pixels[i], second.pixels[i]));
    shared.reveal(observers[0], 8);
    const int secondCenter = shared.index(observers[1].x, observers[1].z);
    assert(shared.revision == 2 && shared.pixels[secondCenter * 2] == 0 &&
           shared.pixels[secondCenter * 2 + 1] == 255);
}

static void checkResidentTerrain() {
    VisibilityMap map;
    Scene s(0, false);
    s.dynamicWorld = true;
    s.add(MeshType::Terrain, {16, 0, 16}, {32, 1, 32}, {.5f, .5f, .5f});
    map.rebuild(s);
    assert(map.residentCell(map.index(31.8f, 15)));
    assert(!map.residentCell(map.index(32, 15)));
    assert(!map.walkable(map.index(-1, 15)));

    ScoutGroup group;
    group.selected.fill(true);
    assert(group.command(map, {31.8f, 0, 15.3f}));
    const auto accepted = group.scouts;
    assert(!group.command(map, {32, 0, 15.3f}));
    s.add(MeshType::Terrain, {80, 0, 16}, {32, 1, 32}, {.5f, .5f, .5f});
    ++s.revision;
    map.rebuild(s);
    assert(map.walkable(map.index(80, 15)));
    assert(!group.command(map, {80, 0, 15}));
    for (size_t i = 0; i < ScoutGroup::size; ++i) {
        assert(group.scouts[i].path == accepted[i].path);
        for (int cell : group.scouts[i].path)
            assert(map.residentCell(cell));
    }

    Camera camera;
    camera.strategy = true;
    camera.target = {48, 0, 15};
    const auto terrain = [&](float x, float z) {
        return map.residentCell(map.index(x, z)) ? 0.f : std::numeric_limits<float>::quiet_NaN();
    };
    assert(!pickStrategy(camera, 0, 0, 16.f / 9, {}, terrain));
    camera.target = {25, 0, 15};
    assert(pickStrategy(camera, 0, 0, 16.f / 9, {}, terrain));
    s.dynamicWorld = false;
    ++s.revision;
    map.rebuild(s);
    assert(map.walkable(map.index(48, 15)));
}

int main() {
    checkPicking();
    checkScoutOrder();
    checkGroupOrders();
    checkGroupVisibility();
    checkResidentTerrain();
    std::cout << "PASS: strategy picking, separate formation, shared visibility and resident terrain\n";
}
