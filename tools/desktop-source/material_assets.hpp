#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace pl {
struct MaterialPixels {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> colorHeight, normalRough;
    float heightAt(float u, float v) const {
        u -= std::floor(u);
        v -= std::floor(v);
        int x = std::min(int(u * width), int(width) - 1), y = std::min(int(v * height), int(height) - 1);
        return colorHeight[(size_t(y) * width + x) * 4 + 3] / 255.f;
    }
};
inline MaterialPixels readMaterial(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Missing material: " + path.string());
    std::array<uint8_t, 16> h{};
    file.read(reinterpret_cast<char *>(h.data()), h.size());
    auto integer = [&](int i) {
        return uint32_t(h[i]) | (uint32_t(h[i + 1]) << 8) | (uint32_t(h[i + 2]) << 16) |
               (uint32_t(h[i + 3]) << 24);
    };
    if (!file || h[0] != 'P' || h[1] != 'L' || h[2] != 'T' || h[3] != 'X' || integer(4) != 1)
        throw std::runtime_error("Invalid material header: " + path.string());
    MaterialPixels p;
    p.width = integer(8);
    p.height = integer(12);
    if (p.width != p.height || p.width < 16 || p.width > 2048 || (p.width & (p.width - 1)))
        throw std::runtime_error("Invalid material dimensions: " + path.string());
    size_t bytes = size_t(p.width) * p.height * 4;
    p.colorHeight.resize(bytes);
    p.normalRough.resize(bytes);
    file.read(reinterpret_cast<char *>(p.colorHeight.data()), bytes);
    file.read(reinterpret_cast<char *>(p.normalRough.data()), bytes);
    if (!file || file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Invalid material payload: " + path.string());
    return p;
}
} // namespace pl
