#pragma once
#include "visibility.hpp"
#include <limits>

namespace pl {
struct ScoutGroup {
    static constexpr size_t size = 3;
    std::array<Scout, size> scouts;
    std::array<bool, size> selected{};

    ScoutGroup() {
        constexpr std::array<V3, size> spawns{{{9, 0, 7}, {13, 0, 7}, {13, 0, 3}}};
        for (size_t i = 0; i < size; ++i) {
            auto &scout = scouts[i];
            scout.position = spawns[i];
            scout.position.y = terrainHeight(scout.position.x, scout.position.z);
            scout.autoPatrol = false;
        }
    }
    void select(size_t index, bool add) {
        if (index >= size)
            return;
        if (!add)
            selected.fill(false);
        selected[index] = true;
    }
    size_t selectionCount() const { return std::count(selected.begin(), selected.end(), true); }

    bool command(const VisibilityMap &map, V3 anchor) {
        const int center = map.index(anchor.x, anchor.z);
        const size_t count = selectionCount();
        if (!count || !map.walkable(center))
            return false;
        if (count == 1) {
            const auto index = std::find(selected.begin(), selected.end(), true) - selected.begin();
            return scouts[index].command(map, anchor);
        }

        std::array<size_t, size> members{};
        size_t next = 0;
        V3 centroid{};
        for (size_t i = 0; i < size; ++i) {
            if (!selected[i])
                continue;
            members[next++] = i;
            centroid = centroid + scouts[i].position * (1.f / count);
        }
        const V3 direction = anchor - centroid;
        V3 lateral = unit({-direction.z, 0, direction.x});
        if (length(lateral) == 0)
            lateral = {1, 0, 0};
        std::stable_sort(members.begin(), members.begin() + count, [&](size_t a, size_t b) {
            return dot(scouts[a].position, lateral) < dot(scouts[b].position, lateral);
        });

        // Match separate, reachable cells near the desired line across the direction of travel.
        std::array<int, 9> cells{};
        std::array<V3, 9> points{};
        size_t slots = 0;
        const V3 cellCenter = map.position(center);
        const float spacing = VisibilityMap::cell;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const V3 point = cellCenter + V3{dx * spacing, 0, dz * spacing};
                const int cell = map.index(point.x, point.z);
                if (!map.walkable(cell))
                    continue;
                cells[slots] = cell;
                points[slots++] = cell == center ? anchor : point;
            }
        if (slots < count)
            return false;

        std::array<int, size> starts{};
        std::array<std::vector<int>, size> routes;
        std::array<std::array<float, 9>, size> costs{};
        for (size_t row = 0; row < count; ++row) {
            starts[row] = scouts[members[row]].startCell(map);
            routes[row] = Scout::search(map, starts[row], cells.data(), slots);
            const V3 ideal = anchor + lateral * ((float(row) - float(count - 1) * .5f) * spacing);
            for (size_t slot = 0; slot < slots; ++slot) {
                const V3 offset = points[slot] - ideal;
                costs[row][slot] = offset.x * offset.x + offset.z * offset.z;
            }
        }
        float bestCost = std::numeric_limits<float>::infinity();
        std::array<size_t, size> trial{}, best{};
        auto assign = [&](auto &self, size_t row, unsigned used, float cost) -> void {
            if (cost >= bestCost)
                return;
            if (row == count) {
                best = trial;
                bestCost = cost;
                return;
            }
            for (size_t slot = 0; slot < slots; ++slot) {
                const unsigned bit = 1u << slot;
                if ((used & bit) || routes[row][cells[slot]] < 0)
                    continue;
                trial[row] = slot;
                self(self, row + 1, used | bit, cost + costs[row][slot]);
            }
        };
        assign(assign, 0, 0, 0);
        if (!std::isfinite(bestCost))
            return false;

        // Stage each route before replacing the current orders as a group.
        auto ordered = scouts;
        for (size_t row = 0; row < count; ++row) {
            const size_t slot = best[row];
            ordered[members[row]].accept(map, starts[row], cells[slot], points[slot], routes[row]);
        }
        scouts = std::move(ordered);
        return true;
    }
};
} // namespace pl
