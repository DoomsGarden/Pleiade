#include "../tools/desktop-source/space_position.hpp"
#include <iostream>

using namespace pl;

namespace {
constexpr double size = SpacePosition::sectorMeters;
constexpr double half = SpacePosition::halfSectorMeters;

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void expectNear(double actual, double expected, const char *message) {
    require(std::abs(actual - expected) < .000001, message);
}
template <class Error, class F> void expectError(const F &operation, const char *message) {
    try {
        operation();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error(message);
}

void crossSector() {
    const SpacePosition start({2, -4, 7}, {half - .25, -half + .125, 0});
    const SpacePosition end = start.translated({.5, -.25, 2 * size + 1});
    const auto sector = end.sector();
    const auto local = end.localMeters();
    require(sector.x == 3 && sector.y == -5 && sector.z == 9, "cross sectors in both directions");
    expectNear(local.x, -half + .25, "positive sector carry");
    expectNear(local.y, half - .125, "negative sector carry");
    expectNear(local.z, 1, "move by more than one sector");
    const auto relative = end.relativeTo(start);
    expectNear(relative.x, .5, "relative distance across positive carry");
    expectNear(relative.y, -.25, "relative distance across negative carry");
    expectNear(relative.z, 2 * size + 1, "farther relative distance");
    expectNear(start.relativeTo(end).x, -.5, "relative distance is symmetric under sign change");
    require(SpacePosition({}, {half, -half, 0}).sector().x == 1, "positive half is exclusive");
    require(SpacePosition({}, {half, -half, 0}).sector().y == 0, "negative half is inclusive");
    const auto justInside = std::nextafter(half, 0.);
    require(SpacePosition({}, {justInside, 0, 0}).sector().x == 0, "do not round boundary early");
}

void astronomicalPrecision() {
    constexpr double lightYearMeters = 9'460'730'472'580'800.;
    for (const auto lightYears : {50'000, 2'500'000}) {
        const auto sector = static_cast<std::int64_t>(lightYears * lightYearMeters / size);
        const SpacePosition camera({sector, -sector, 0}, {half - .014, 2.1, -88.5});
        const SpacePosition object = camera.translated({.020, -1.25, .0012});
        require(object.sector().x == sector + 1, "astronomical movement crosses the sector");
        const V3 drawPosition = object.relativeToCamera(camera);
        expectNear(drawPosition.x, .020, "two centimetres across a sector far from origin");
        expectNear(drawPosition.y, -1.25, "metre precision far from origin");
        expectNear(drawPosition.z, .0012, "millimetre precision far from origin");
    }

    // Converting the two sector addresses to doubles before subtraction
    // would collapse these neighbours into the same address.
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    const SpacePosition edge({max - 1, 0, 0}, {half - .025, 0, 0});
    expectNear(edge.translated({.075, 0, 0}).relativeTo(edge).x, .075, "adjacency at sector limit");
    const auto largeMove = SpacePosition({}, {.01, 0, 0}).translated({size * 0x1p60, 0, 0});
    expectNear(largeMove.localMeters().x, .01, "long translations preserve local displacement");
}

void limits() {
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    const SpacePosition top({max, 0, 0}, {-size * .49, 0, 0});
    const SpacePosition bottom({min, 0, 0}, {size * .49, 0, 0});
    require(top.translated({size * .52, 0, 0}).sector().x == max, "fitting upper carry");
    require(bottom.translated({-size * .52, 0, 0}).sector().x == min, "fitting lower carry");
    expectError<std::overflow_error>([&] { top.translated({2 * size, 0, 0}); }, "upper bound");
    expectError<std::overflow_error>([&] { bottom.translated({-2 * size, 0, 0}); }, "lower bound");
    expectError<std::overflow_error>([] { SpacePosition({max, 0, 0}, {half, 0, 0}); },
                                     "initial offset cannot overflow");
    expectError<std::overflow_error>([] { SpacePosition().translated({size * 0x1p63, 0, 0}); },
                                     "displacement must fit");
    require(SpacePosition().translated({-size * 0x1p63, 0, 0}).sector().x == min,
            "minimum signed sector is representable");
    const double distance = top.relativeTo(bottom).x;
    require(std::isfinite(distance) && distance > size * 0x1p63, "opposite signed sector limits");
    require(bottom.relativeTo(top).x == -distance, "opposite distance keeps its sign");

    constexpr double infinity = std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    expectError<std::invalid_argument>([] { SpacePosition({}, {0, infinity, 0}); }, "infinite input");
    expectError<std::invalid_argument>([&] { top.translated({0, 0, nan}); }, "NaN displacement");
    require(top.sector().x == max && top.localMeters().x == -size * .49, "failed move is atomic");
}
} // namespace

int main() {
    try {
        crossSector();
        astronomicalPrecision();
        limits();
        std::cout << "PASS: astronomical sectors, sub-metre rebasing, boundaries and range errors\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
