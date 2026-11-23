#include "../tools/desktop-source/strategy_selection.hpp"
#include "../tools/desktop-source/visibility.hpp"
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

int main() {
    checkPicking();
    checkScoutOrder();
    std::cout << "PASS: strategy projection, closest entity, terrain occlusion, scout orders\n";
}
