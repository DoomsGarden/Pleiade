#pragma once
#include "math.hpp"
#include <cstdint>
#include <limits>

namespace pl {
struct SpaceVector {
    double x = 0, y = 0, z = 0;
};
struct SpaceSector {
    std::int64_t x = 0, y = 0, z = 0;
};

// Signed sector addresses reach about 4.2e12 light-years from the origin on each
// axis. Double-precision offsets stay in [-halfSectorMeters, halfSectorMeters).
class SpacePosition {
  public:
    static constexpr double sectorMeters = 0x1p32;
    static constexpr double halfSectorMeters = sectorMeters * .5;

  private:
    struct Axis {
        std::int64_t sector = 0;
        double local = 0;
    };
    std::array<Axis, 3> axes{};

    static std::int64_t addSectors(std::int64_t base, std::int64_t offset) {
        constexpr auto min = std::numeric_limits<std::int64_t>::min();
        constexpr auto max = std::numeric_limits<std::int64_t>::max();
        if ((offset > 0 && base > max - offset) || (offset < 0 && base < min - offset))
            throw std::overflow_error("Space sector out of range");
        return base + offset;
    }
    static Axis split(double meters) {
        if (!std::isfinite(meters))
            throw std::invalid_argument("Non-finite space position");
        double sectors = std::trunc(meters / sectorMeters);
        double local = meters - sectors * sectorMeters;
        if (local >= halfSectorMeters) {
            ++sectors;
            local -= sectorMeters;
        } else if (local < -halfSectorMeters) {
            --sectors;
            local += sectorMeters;
        }
        constexpr double limit = 0x1p63;
        if (sectors < -limit || sectors >= limit)
            throw std::overflow_error("Space displacement out of range");
        return {static_cast<std::int64_t>(sectors), local};
    }
    static Axis normalized(std::int64_t sector, double localMeters) {
        const Axis offset = split(localMeters);
        return {addSectors(sector, offset.sector), offset.local};
    }
    static Axis translatedAxis(Axis original, double meters) {
        // Split a long displacement before adding the local terms so that a
        // small local offset survives even when the displacement is enormous.
        const Axis displacement = split(meters);
        const Axis carry = split(original.local + displacement.local);
        const bool opposing = (displacement.sector > 0 && carry.sector < 0) ||
                              (displacement.sector < 0 && carry.sector > 0);
        const auto sector = opposing
                                ? addSectors(original.sector, displacement.sector + carry.sector)
                                : addSectors(addSectors(original.sector, displacement.sector),
                                             carry.sector);
        return {sector, carry.local};
    }
    static double difference(Axis target, Axis origin) {
        if (target.sector == origin.sector)
            return target.local - origin.local;
        if (target.sector < origin.sector)
            return -difference(origin, target);
        // Unsigned subtraction also works when the distance between two
        // signed sectors is wider than the range of int64_t.
        const auto sectors = static_cast<std::uint64_t>(target.sector) -
                             static_cast<std::uint64_t>(origin.sector);
        if (sectors == 1)
            return (halfSectorMeters - origin.local) + (target.local + halfSectorMeters);
        return static_cast<double>(sectors) * sectorMeters + (target.local - origin.local);
    }

  public:
    SpacePosition() = default;
    explicit SpacePosition(SpaceSector sector, SpaceVector localMeters = {})
        : axes{normalized(sector.x, localMeters.x), normalized(sector.y, localMeters.y),
               normalized(sector.z, localMeters.z)} {}

    SpaceSector sector() const { return {axes[0].sector, axes[1].sector, axes[2].sector}; }
    SpaceVector localMeters() const { return {axes[0].local, axes[1].local, axes[2].local}; }

    SpacePosition translated(SpaceVector meters) const {
        SpacePosition result;
        result.axes = {translatedAxis(axes[0], meters.x), translatedAxis(axes[1], meters.y),
                       translatedAxis(axes[2], meters.z)};
        return result;
    }
    SpaceVector relativeTo(const SpacePosition &origin) const {
        return {difference(axes[0], origin.axes[0]), difference(axes[1], origin.axes[1]),
                difference(axes[2], origin.axes[2])};
    }
    // Cast only after rebasing. Floats retain their useful precision for
    // nearby geometry. Distant rendering still needs its own scale or LOD.
    V3 relativeToCamera(const SpacePosition &camera) const {
        const auto relative = relativeTo(camera);
        return {static_cast<float>(relative.x), static_cast<float>(relative.y),
                static_cast<float>(relative.z)};
    }
};
} // namespace pl
