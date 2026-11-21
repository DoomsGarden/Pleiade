#include "renderer.hpp"
#include "building_geometry.hpp"
#include "canopy_layers.hpp"
#include "material_assets.hpp"
#include "shaders.hpp"
#include "shadow_fit.hpp"
#include "shore.hpp"
#include "terrain_lod.hpp"
#include "vegetation_meshes.hpp"
#include "visibility.hpp"
#include <chrono>
#include <cstddef>
#include <fstream>

namespace pl {
using RenderClock = std::chrono::steady_clock;
static double renderMs(RenderClock::time_point a, RenderClock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

GLuint Renderer::program(const char *vs, const char *fs) {
    auto compile = [&](GLenum type, const char *code) {
        GLuint s = gl.CreateShader(type);
        gl.ShaderSource(s, 1, &code, nullptr);
        gl.CompileShader(s);
        GLint ok = 0;
        gl.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char msg[8192]{};
            gl.GetShaderInfoLog(s, sizeof(msg), nullptr, msg);
            gl.DeleteShader(s);
            throw std::runtime_error(msg);
        }
        return s;
    };
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = 0;
    try {
        f = compile(GL_FRAGMENT_SHADER, fs);
    } catch (...) {
        gl.DeleteShader(v);
        throw;
    }
    GLuint p = gl.CreateProgram();
    gl.AttachShader(p, v);
    gl.AttachShader(p, f);
    gl.LinkProgram(p);
    gl.DeleteShader(v);
    gl.DeleteShader(f);
    GLint ok = 0;
    gl.GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char msg[8192]{};
        gl.GetProgramInfoLog(p, sizeof(msg), nullptr, msg);
        gl.DeleteProgram(p);
        throw std::runtime_error(msg);
    }
    return p;
}
Mesh Renderer::makeMesh(const std::vector<Vertex> &verts, const std::vector<uint32_t> &indices) {
    Mesh m;
    m.indices = static_cast<GLsizei>(indices.size());
    gl.GenVertexArrays(1, &m.vao);
    gl.GenBuffers(1, &m.vbo);
    gl.GenBuffers(1, &m.ebo);
    gl.GenBuffers(1, &m.instances);
    gl.BindVertexArray(m.vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, m.vbo);
    gl.BufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                           reinterpret_cast<void *>(offsetof(Vertex, n)));
    gl.EnableVertexAttribArray(8);
    gl.VertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                           reinterpret_cast<void *>(offsetof(Vertex, u)));
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(),
                  GL_STATIC_DRAW);
    gl.BindBuffer(GL_ARRAY_BUFFER, m.instances);
    for (int i = 0; i < 4; ++i) {
        gl.EnableVertexAttribArray(2 + i);
        gl.VertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                               reinterpret_cast<void *>(i * 4 * sizeof(float)));
        gl.VertexAttribDivisor(2 + i, 1);
    }
    for (int i = 0; i < 2; ++i) {
        gl.EnableVertexAttribArray(6 + i);
        gl.VertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                               reinterpret_cast<void *>(sizeof(M4) + i * sizeof(V4)));
        gl.VertexAttribDivisor(6 + i, 1);
    }
    gl.BindVertexArray(0);
    return m;
}
Renderer::Renderer(int w, int h, int shadowResolution, bool gpuTiming, const std::string &assetDirectory)
    : shadowSize(shadowResolution), gpuTimingEnabled(gpuTiming) {
    gl.load();
    vendor = reinterpret_cast<const char *>(gl.GetString(GL_VENDOR));
    gpu = reinterpret_cast<const char *>(gl.GetString(GL_RENDERER));
    api = reinterpret_cast<const char *>(gl.GetString(GL_VERSION));
    std::filesystem::path assets = assetDirectory;
    if (assets.empty()) {
        char *base = SDL_GetBasePath();
        if (!base)
            throw std::runtime_error("Cannot locate executable directory");
        assets = std::filesystem::path(base) / ".." / "assets" / "runtime";
        SDL_free(base);
    }
    std::array<MaterialPixels, 3> materialData{readMaterial(assets / "ground.pltex"),
                                               readMaterial(assets / "bark.pltex"),
                                               readMaterial(assets / "rock.pltex")};
    gl.GenTextures(6, materialTextures.data());
    for (int i = 0; i < 3; ++i)
        for (int channel = 0; channel < 2; ++channel) {
            bindTexture(materialTextures[i * 2 + channel], 5 + i * 2 + channel);
            auto &data = materialData[i];
            gl.TexImage2D(GL_TEXTURE_2D, 0, channel ? GL_RGBA8 : GL_SRGB8_ALPHA8, data.width,
                          data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          channel ? data.normalRough.data() : data.colorHeight.data());
            gl.GenerateMipmap(GL_TEXTURE_2D);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    constexpr int layerSize = 256, layerCount = int(SurfaceLayer::Count);
    const std::array layerFiles{assets / "surfaces-color.rgba", assets / "surfaces-normal.rgba"};
    if (std::filesystem::exists(layerFiles[0]) && std::filesystem::exists(layerFiles[1])) {
        gl.GenTextures(2, surfaceTextures.data());
        std::vector<uint8_t> pixels(layerSize * layerSize * layerCount * 4);
        for (int i = 0; i < 2; ++i) {
            std::ifstream input(layerFiles[i], std::ios::binary);
            if (!input.read(reinterpret_cast<char *>(pixels.data()), pixels.size()) ||
                input.peek() != std::char_traits<char>::eof())
                throw std::runtime_error("Invalid surface layers: " + layerFiles[i].string());
            gl.BindTexture(GL_TEXTURE_2D_ARRAY, surfaceTextures[i]);
            gl.TexImage3D(GL_TEXTURE_2D_ARRAY, 0, i == 0 ? GL_SRGB8_ALPHA8 : GL_RGBA8,
                          layerSize, layerSize, layerCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            gl.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            gl.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.GenerateMipmap(GL_TEXTURE_2D_ARRAY);
        }
    }
    sceneProgram = program(shader::geometry, shader::material);
    std::string impostorSource(shader::material);
    impostorSource.insert(impostorSource.find('\n') + 1, "#define DEPTH_IMPOSTOR\n");
    impostorProgram = program(shader::geometry, impostorSource.c_str());
    const char *names[] = {
        "uVP",         "uLightVP",    "uView",    "uEye",  "uShadows", "uShadow", "uVisibilityEnabled",
        "uVisibility", "uFogDensity", "uNatural", "uTime", "uCanopy"};
    for (int i = 0; i < 12; ++i)
        impostorLoc[i] = gl.GetUniformLocation(impostorProgram, names[i]);
    shadowProgram = program(shader::geometry, shader::shadow);
    for (GLuint id : {sceneProgram, shadowProgram, impostorProgram}) {
        gl.UseProgram(id);
        gl.Uniform1i(gl.GetUniformLocation(id, "uSurfaceColor"), 14);
        gl.Uniform1i(gl.GetUniformLocation(id, "uSurfaceNormal"), 15);
        gl.Uniform1i(gl.GetUniformLocation(id, "uSurfaceMaps"), surfaceTextures[0] != 0);
    }
    compositeProgram = program(shader::fullscreen, shader::composite);
    uiProgram = program(shader::uiVert, shader::uiFrag);
    loc = {gl.GetUniformLocation(sceneProgram, "uVP"),
           gl.GetUniformLocation(sceneProgram, "uEye"),
           gl.GetUniformLocation(sceneProgram, "uLightVP"),
           gl.GetUniformLocation(sceneProgram, "uView"),
           gl.GetUniformLocation(sceneProgram, "uShadow"),
           gl.GetUniformLocation(sceneProgram, "uShadows")};
    visibilityEnabled = gl.GetUniformLocation(sceneProgram, "uVisibilityEnabled");
    visibilitySampler = gl.GetUniformLocation(sceneProgram, "uVisibility");
    shadowVP = gl.GetUniformLocation(shadowProgram, "uVP");
    sceneTime = gl.GetUniformLocation(sceneProgram, "uTime");
    shadowTime = gl.GetUniformLocation(shadowProgram, "uTime");
    fogDensity = gl.GetUniformLocation(sceneProgram, "uFogDensity");
    materialsLoc = gl.GetUniformLocation(sceneProgram, "uMaterials");
    wetLoc = gl.GetUniformLocation(sceneProgram, "uWet");
    wetTimeLoc = gl.GetUniformLocation(sceneProgram, "uWetTime");
    waveLoc = gl.GetUniformLocation(sceneProgram, "uWaveStrength");
    gl.UseProgram(sceneProgram);
    const char *maps[] = {"uGroundColor", "uGroundNormal", "uBarkColor",
                          "uBarkNormal",  "uRockColor",    "uRockNormal"};
    for (int i = 0; i < 6; ++i)
        gl.Uniform1i(gl.GetUniformLocation(sceneProgram, maps[i]), 5 + i);
    const char *waterNames[] = {"uWater",    "uWaterTime",     "uCameraEye", "uCameraRight",
                                "uCameraUp", "uCameraForward", "uWaterSSR", "uWaveStrength"};
    for (int i = 0; i < 8; ++i)
        waterLoc[i] = gl.GetUniformLocation(compositeProgram, waterNames[i]);
    naturalLoc = gl.GetUniformLocation(sceneProgram, "uNatural");
    contactLoc = gl.GetUniformLocation(compositeProgram, "uContact");
    projectionLoc = gl.GetUniformLocation(compositeProgram, "uProjectionScale");
    aaLoc = gl.GetUniformLocation(compositeProgram, "uAA");
    exposureLoc = gl.GetUniformLocation(compositeProgram, "uExposure");
    uiSize = gl.GetUniformLocation(uiProgram, "uSize");
    gl.UseProgram(compositeProgram);
    gl.Uniform1i(gl.GetUniformLocation(compositeProgram, "uDirect"), 0);
    gl.Uniform1i(gl.GetUniformLocation(compositeProgram, "uAmbient"), 1);
    gl.Uniform1i(gl.GetUniformLocation(compositeProgram, "uDepth"), 2);
    gl.Uniform1i(gl.GetUniformLocation(compositeProgram, "uNormal"), 3);
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const V3 normals[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (V3 n : normals) {
        V3 u = std::abs(n.y) > .5f ? V3{1, 0, 0} : unit(cross({0, 1, 0}, n));
        V3 v = cross(n, u);
        uint32_t start = static_cast<uint32_t>(vertices.size());
        for (auto s : std::array<V3, 4>{{{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}}})
            vertices.push_back({(n + u * s.x + v * s.y) * .5f, n});
        for (uint32_t i : {0, 1, 2, 0, 2, 3})
            indices.push_back(start + i);
    }
    meshes[0] = makeMesh(vertices, indices);
    vertices.clear();
    indices.clear();
    constexpr int slices = 48, stacks = 24;
    for (int j = 0; j <= stacks; ++j)
        for (int i = 0; i <= slices; ++i) {
            float a = i * 2 * pi / slices, b = j * pi / stacks;
            V3 n{std::sin(b) * std::cos(a), std::cos(b), std::sin(b) * std::sin(a)};
            vertices.push_back({n * .5f, n});
        }
    for (int j = 0; j < stacks; ++j)
        for (int i = 0; i < slices; ++i) {
            uint32_t a = j * (slices + 1) + i, b = a + slices + 1;
            indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    meshes[1] = makeMesh(vertices, indices);
    // Two shared foliage meshes; the far tier has one quarter of the triangles.
    for (int tier = 0; tier < 2; ++tier) {
        vertices.clear();
        indices.clear();
        int slices = tier ? 8 : 16, stacks = tier ? 5 : 10;
        for (int j = 0; j <= stacks; ++j)
            for (int i = 0; i <= slices; ++i) {
                float a = i * 2 * pi / slices, b = j * pi / stacks;
                V3 n{std::sin(b) * std::cos(a), std::cos(b), std::sin(b) * std::sin(a)};
                vertices.push_back({n * .5f, n});
            }
        for (int j = 0; j < stacks; ++j)
            for (int i = 0; i < slices; ++i) {
                uint32_t a = j * (slices + 1) + i, b = a + slices + 1;
                indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        meshes[static_cast<int>(tier ? MeshType::FoliageFar : MeshType::Foliage)] =
            makeMesh(vertices, indices);
    }
    vertices.clear();
    indices.clear();
    for (int blade = 0; blade < 3; ++blade) {
        float a = blade * pi / 3;
        V3 u{std::cos(a), 0, std::sin(a)}, n{-std::sin(a), 0, std::cos(a)};
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({u * (-.23f) + V3{0, -.5f, 0}, n});
        vertices.push_back({u * .23f + V3{0, -.5f, 0}, n});
        vertices.push_back({u * .12f + V3{0, .1f, 0}, n});
        vertices.push_back({u * .03f + V3{0, .5f, 0}, n});
        indices.insert(indices.end(),
                       {b, b + 1, b + 2, b, b + 2, b + 3, b + 2, b + 1, b, b + 3, b + 2, b});
    }
    meshes[static_cast<int>(MeshType::Grass)] = makeMesh(vertices, indices);
    vertices.clear();
    indices.clear();
    constexpr int grid = 16;
    for (int z = 0; z <= grid; ++z)
        for (int x = 0; x <= grid; ++x)
            vertices.push_back({{float(x) / grid - .5f, 0, float(z) / grid - .5f}, {0, 1, 0}});
    for (int z = 0; z < grid; ++z)
        for (int x = 0; x < grid; ++x) {
            uint32_t a = z * (grid + 1) + x, b = a + grid + 1;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    meshes[static_cast<int>(MeshType::Terrain)] = makeMesh(vertices, indices);
    auto coarse = coarseTerrainTopology();
    vertices.clear();
    for (auto p : coarse.positions)
        vertices.push_back({p, {0, 1, 0}});
    meshes[static_cast<int>(MeshType::TerrainCoarse)] = makeMesh(vertices, coarse.indices);
    vertices.clear();
    indices.clear();
    const V3 roof[] = {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {0, .5f, -.5f},
                       {-.5f, -.5f, .5f},  {.5f, -.5f, .5f},  {0, .5f, .5f}};
    const int faces[][3] = {{0, 2, 1}, {3, 4, 5}, {0, 3, 5}, {0, 5, 2},
                            {1, 2, 5}, {1, 5, 4}, {0, 1, 4}, {0, 4, 3}};
    for (auto &face : faces) {
        V3 a = roof[face[0]], b = roof[face[1]], c = roof[face[2]], n = unit(cross(b - a, c - a));
        for (V3 p : {a, b, c}) {
            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({p, n});
        }
    }
    meshes[static_cast<int>(MeshType::Roof)] = makeMesh(vertices, indices);
    hipRoofMesh(vertices, indices);
    meshes[static_cast<int>(MeshType::HipRoof)] = makeMesh(vertices, indices);
    // Shared reference meshes: no per-tree vertex buffer duplication.
    auto tri = [&](V3 a, V3 b, V3 c, bool twoSided = false) {
        V3 n = unit(cross(b - a, c - a));
        uint32_t start = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {{a, n}, {b, n}, {c, n}});
        indices.insert(indices.end(), {start, start + 1, start + 2});
        if (twoSided)
            indices.insert(indices.end(), {start + 2, start + 1, start});
    };
    auto textTriangle = [&](Vertex a, Vertex b, Vertex c, bool twoSided) {
        uint32_t start = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {a, b, c});
        indices.insert(indices.end(), {start, start + 1, start + 2});
        if (twoSided) indices.insert(indices.end(), {start + 2, start + 1, start});
    };
    for (MeshType mesh : vegetationMeshes) {
        vertices.clear();
        indices.clear();
        buildVegetationMesh(mesh, textTriangle);
        meshes[static_cast<int>(mesh)] = makeMesh(vertices, indices);
    }
    vertices.clear();
    indices.clear();
    for (V3 face : normals) {
        const V3 u = std::abs(face.y) > .5f ? V3{1, 0, 0} : unit(cross({0, 1, 0}, face));
        const V3 v = cross(face, u);
        constexpr float radius = .061f, inner = .5f - radius;
        constexpr float samples[]{-.5f, -inner, inner, .5f};
        auto bevel = [&](float x, float y) {
            const V3 p = face * .5f + u * x + v * y;
            const V3 core{std::clamp(p.x, -inner, inner), std::clamp(p.y, -inner, inner),
                          std::clamp(p.z, -inner, inner)};
            const V3 n = unit(p - core);
            return Vertex{core + n * radius, n, x + .5f, y + .5f};
        };
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) {
                Vertex a = bevel(samples[x], samples[y]), b = bevel(samples[x + 1], samples[y]);
                Vertex c = bevel(samples[x], samples[y + 1]), d = bevel(samples[x + 1], samples[y + 1]);
                textTriangle(a, b, c, false);
                textTriangle(b, d, c, false);
            }
    }
    meshes[static_cast<int>(MeshType::BeveledBox)] = makeMesh(vertices, indices);
    for (int tier = 0; tier < 2; ++tier) {
        vertices.clear();
        indices.clear();
        int count = tier ? 45 : 150;
        for (int leaf = 0; leaf < count; ++leaf) {
            // Far leaves are an exact subset: same position, orientation and size.
            int i = leaf * 150 / count;
            float f = (i + .5f) / 150, y = 1 - 2 * f, a = i * 2.39996323f;
            float r = std::sqrt(std::max(0.f, 1 - y * y));
            V3 dir{r * std::cos(a), y, r * std::sin(a)};
            float shell = .30f + .065f * std::sin(i * 7.31f);
            V3 center = dir * shell;
            V3 normal = unit(dir + V3{.2f, .65f, .1f});
            V3 u = unit(cross(std::abs(normal.y) > .95f ? V3{1, 0, 0} : V3{0, 1, 0}, normal));
            V3 v = cross(normal, u);
            float size = .085f;
            V3 a0 = center - u * size, b = center + v * (size * .52f), c = center + u * size,
               d = center - v * (size * .52f);
            tri(a0, b, c, true);
            tri(a0, c, d, true);
        }
        meshes[static_cast<int>(tier ? MeshType::LeavesFar : MeshType::Leaves)] =
            makeMesh(vertices, indices);
    }
    vertices.clear();
    indices.clear();
    for (auto t : detailedLeaves())
        tri(t.a, t.b, t.c, true);
    meshes[static_cast<int>(MeshType::LeavesDetail)] = makeMesh(vertices, indices);
    vertices.clear();
    indices.clear();
    tri({-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0});
    tri({-.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0});
    meshes[static_cast<int>(MeshType::CanopyImpostor)] = makeMesh(vertices, indices);
    auto atlas = bakeCanopyAtlas();
    gl.GenTextures(1, &canopyTexture);
    bindTexture(canopyTexture, 4);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, canopyAzimuths * canopyTile,
                  canopyElevations * canopyTile, 0, GL_RGBA, GL_FLOAT, atlas.data());
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    vertices.clear();
    indices.clear();
    for (int i = 0; i < 9; ++i) {
        float a = i * 2.39996f;
        V3 u{std::cos(a), 0, std::sin(a)}, v{-u.z, 0, u.x};
        V3 base = u * (.12f + .022f * (i % 3)) + V3{0, -.5f, 0};
        V3 mid = base + V3{0, .40f + .035f * (i % 4), 0} + v * .08f;
        V3 tip = base + V3{0, .75f + .025f * (i % 7), 0} + v * .27f;
        tri(base - u * .025f, base + u * .025f, mid + u * .014f, true);
        tri(base - u * .025f, mid + u * .014f, mid - u * .014f, true);
        tri(mid - u * .014f, mid + u * .014f, tip, true);
    }
    meshes[static_cast<int>(MeshType::Meadow)] = makeMesh(vertices, indices);
    vertices.clear();
    indices.clear();
    for (int j = 0; j < 6; ++j)
        for (int i = 0; i < 10; ++i) {
            auto point = [](int i, int j) {
                float a = i * 2 * pi / 10, b = j * pi / 6;
                float r = .43f + .04f * std::sin(i * 4.f + j * 9.f);
                return V3{r * std::sin(b) * std::cos(a), r * std::cos(b), r * std::sin(b) * std::sin(a)};
            };
            tri(point(i, j), point(i + 1, j), point(i, j + 1));
            tri(point(i + 1, j), point(i + 1, j + 1), point(i, j + 1));
        }
    meshes[static_cast<int>(MeshType::Rock)] = makeMesh(vertices, indices);
    vertices.clear();
    indices.clear();
    for (int z = 0; z <= 96; ++z)
        for (int x = 0; x <= 96; ++x) {
            float wx = -24 + x * .5f, wz = -24 + z * .5f;
            vertices.push_back({{wx, shoreHeight(wx, wz), wz}, shoreNormal(wx, wz)});
        }
    for (int z = 0; z < 96; ++z)
        for (int x = 0; x < 96; ++x) {
            uint32_t a = z * 97 + x, b = a + 97;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    meshes[static_cast<int>(MeshType::ShoreGround)] = makeMesh(vertices, indices);
    vertices.clear();
    indices.clear();
    auto rockPoint = [&](int i, int j) {
        float u = float(i) / 48, v = float(j) / 24, a = u * 2 * pi, b = v * pi;
        float radius = .42f + .035f * std::sin(a * 3) * std::sin(b * 4) +
                       .055f * std::sin(b) * (materialData[2].heightAt(u * 2, v) - .5f);
        return V3{radius * std::sin(b) * std::cos(a), radius * std::cos(b),
                  radius * std::sin(b) * std::sin(a)};
    };
    for (int j = 0; j <= 24; ++j)
        for (int i = 0; i <= 48; ++i) {
            V3 pos = rockPoint(i, j);
            V3 n = unit(cross(rockPoint(i, j + 1) - rockPoint(i, j - 1),
                              rockPoint(i + 1, j) - rockPoint(i - 1, j)));
            if (dot(n, pos) < 0)
                n = n * -1;
            if (length(n) < .1f)
                n = unit(pos);
            vertices.push_back({pos, n});
        }
    for (int j = 0; j < 24; ++j)
        for (int i = 0; i < 48; ++i) {
            uint32_t a = j * 49 + i, b = a + 49;
            indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    meshes[static_cast<int>(MeshType::RockDetail)] = makeMesh(vertices, indices);
    gl.GenVertexArrays(1, &emptyVAO);
    gl.GenVertexArrays(1, &uiVAO);
    gl.GenBuffers(1, &uiVBO);
    gl.BindVertexArray(uiVAO);
    gl.BindBuffer(GL_ARRAY_BUFFER, uiVBO);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), nullptr);
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                           reinterpret_cast<void *>(offsetof(UIVertex, color)));
    if (gpuTimingEnabled)
        for (auto &q : queries)
            gl.GenQueries(1, &q.id);
    for (int i = 0; i < meshCount; ++i)
        visible[i].reserve(i == static_cast<int>(MeshType::TerrainCoarse) ? 128 : 12000);
    gl.GenFramebuffers(1, &fbo);
    gl.GenTextures(4, targets.data());
    resize(w, h);
    createShadow();
    gl.Enable(GL_DEPTH_TEST);
    gl.DepthFunc(GL_LESS);
    gl.Enable(GL_CULL_FACE);
    gl.check("Renderer startup");
}
Renderer::~Renderer() {
    for (auto m : meshes) {
        gl.DeleteBuffers(1, &m.vbo);
        gl.DeleteBuffers(1, &m.ebo);
        gl.DeleteBuffers(1, &m.instances);
        gl.DeleteVertexArrays(1, &m.vao);
    }
    for (auto p : {sceneProgram, shadowProgram, compositeProgram, uiProgram, impostorProgram})
        gl.DeleteProgram(p);
    for (auto q : queries)
        if (q.id)
            gl.DeleteQueries(1, &q.id);
    if (visibilityTexture)
        gl.DeleteTextures(1, &visibilityTexture);
    gl.DeleteTextures(6, materialTextures.data());
    gl.DeleteTextures(2, surfaceTextures.data());
    gl.DeleteTextures(1, &canopyTexture);
    gl.DeleteTextures(4, targets.data());
    gl.DeleteTextures(1, &shadowTexture);
    gl.DeleteFramebuffers(1, &fbo);
    gl.DeleteFramebuffers(1, &shadowFbo);
    gl.DeleteVertexArrays(1, &emptyVAO);
    gl.DeleteVertexArrays(1, &uiVAO);
    gl.DeleteBuffers(1, &uiVBO);
}
void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0)
        return;
    width = w;
    height = h;
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    for (int i = 0; i < 4; ++i) {
        gl.BindTexture(GL_TEXTURE_2D, targets[i]);
        gl.TexImage2D(GL_TEXTURE_2D, 0, i == 3 ? GL_DEPTH_COMPONENT24 : GL_RGBA16F, w, h, 0,
                      i == 3 ? GL_DEPTH_COMPONENT : GL_RGBA, GL_FLOAT, nullptr);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, i == 3 ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0 + i,
                                GL_TEXTURE_2D, targets[i], 0);
    }
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    gl.DrawBuffers(3, buffers);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete scene framebuffer");
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.check("Resize");
}
void Renderer::createShadow() {
    gl.GenFramebuffers(1, &shadowFbo);
    gl.GenTextures(1, &shadowTexture);
    gl.BindTexture(GL_TEXTURE_2D, shadowTexture);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT,
                  GL_FLOAT, nullptr);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1, 1, 1, 1};
    gl.TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    gl.BindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
    gl.DrawBuffer(GL_NONE);
    gl.ReadBuffer(GL_NONE);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete shadow framebuffer");
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    lightVP = ortho(-31, 31, -31, 31, .1f, 140) * lookAt({-36, 60, 36}, {0, 0, 0});
}
void Renderer::drawMesh(const Mesh &m, const std::vector<Instance> &instances, double *uploadMs,
                        double *drawMs) {
    if (instances.empty())
        return;
    auto begin = uploadMs ? RenderClock::now() : RenderClock::time_point{};
    gl.BindVertexArray(m.vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, m.instances);
    gl.BufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(Instance), instances.data(),
                  GL_STREAM_DRAW);
    auto uploaded = uploadMs ? RenderClock::now() : RenderClock::time_point{};
    gl.DrawElementsInstanced(GL_TRIANGLES, m.indices, GL_UNSIGNED_INT, nullptr,
                             static_cast<GLsizei>(instances.size()));
    if (uploadMs) {
        auto end = RenderClock::now();
        *uploadMs += renderMs(begin, uploaded);
        *drawMs += renderMs(uploaded, end);
    }
}
void Renderer::bindTexture(GLuint t, int unit) {
    gl.ActiveTexture(GL_TEXTURE0 + unit);
    gl.BindTexture(GL_TEXTURE_2D, t);
}
RenderStats Renderer::render(const Scene &scene, const Camera &camera, bool culling, bool shadows,
                             float exposure, bool lod, bool aa, bool regionCulling, bool traceRender,
                             bool terrainLOD, bool natural, bool layers, bool materials, bool water,
                             bool waterSSR, float waveStrength) {
    auto stamp = [&]() { return traceRender ? RenderClock::now() : RenderClock::time_point{}; };
    auto renderBegin = stamp();
    gl.Enable(GL_DEPTH_TEST);
    gl.Enable(GL_CULL_FACE);
    gl.Disable(GL_BLEND);
    for (int i = 0; i < 2; ++i) {
        gl.ActiveTexture(GL_TEXTURE0 + 14 + i);
        gl.BindTexture(GL_TEXTURE_2D_ARRAY, surfaceTextures[i]);
    }
    RenderStats stats;
    auto stateEnd = stamp();
    stats.trace.state = renderMs(renderBegin, stateEnd);
    if (scene.dynamicWorld) {
        V3 c = camera.strategy ? camera.target : camera.pos;
        // Strategy fits visible loaded regions; FPS retains the local volume.
        c.y = 0;
        lightVP = camera.strategy ? strategyShadowVP(scene, camera, float(width) / height, shadowSize)
                                  : ortho(-85, 85, -85, 85, .1f, 250) * lookAt(c + V3{-60, 100, 60}, c);
    }
    auto prepareEnd = stamp(), shadowEnd = prepareEnd;
    if (shadows && (!shadowValid || scene.dynamicWorld)) {
        for (auto &v : visible)
            v.clear();
        Frustum sf(lightVP);
        for (auto &o : scene.objects) {
            if (!o.visible || o.lodClass == 2 || !sf.visible(o.center, o.half)) continue;
            auto mesh=o.mesh;
            if(lod&&surfaceTextures[0]&&vegetationFarMesh(mesh)!=mesh){
                const auto mid=vegetationMiddleMesh(mesh);
                const float px=canopyPixels(camera,float(height),o.center,o.half*2);
                if(mid==mesh)mesh=px<9?vegetationFarMesh(mesh):mesh;
                else if(px<24)mesh=vegetationFarMesh(mesh);
                else if(px<54)mesh=mid;
            }
            Instance part = o.gpu;
            part.surface.z = foliageMaterialForMesh(mesh, part.surface.z);
            visible[static_cast<int>(mesh)].push_back(part);
        }
        prepareEnd = stamp();
        gl.BindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        gl.Viewport(0, 0, shadowSize, shadowSize);
        gl.Clear(GL_DEPTH_BUFFER_BIT);
        gl.UseProgram(shadowProgram);
        gl.UniformMatrix4fv(shadowVP, 1, GL_FALSE, lightVP.v.data());
        gl.Uniform1f(shadowTime, float(scene.time));
        gl.Enable(GL_POLYGON_OFFSET_FILL);
        gl.PolygonOffset(2, 4);
        for (int i = 0; i < meshCount; ++i) {
            drawMesh(meshes[i], visible[i], traceRender ? &stats.trace.shadowUpload : nullptr,
                     traceRender ? &stats.trace.shadowDraw : nullptr);
            if (!visible[i].empty())
                ++stats.shadowDraws;
        }
        gl.Disable(GL_POLYGON_OFFSET_FILL);
        shadowValid = true;
        shadowEnd = stamp();
    }
    stats.trace.shadowPrepare = renderMs(stateEnd, prepareEnd);
    stats.trace.shadowSubmit = renderMs(prepareEnd, shadowEnd);
    stats.total = static_cast<int>(scene.objects.size());
    auto start = std::chrono::steady_clock::now();
    M4 view = camera.view(), vp = camera.projection(float(width) / height) * view;
    Frustum frustum(vp);
    const V3 cameraEye = camera.eye();
    if (layers)
        canopyCache.begin(scene.revision, scene.objects.size(), cameraEye,
                          float(height) / (2 * std::tan((camera.strategy ? 48.f : 62.f) * pi / 360)));
    for (auto &v : visible)
        v.clear();
    auto visit = [&](const Object &o) {
        if (!o.visible) return;
        ++stats.objectCandidates;
        if (visibilityMap && visibilityCulling && o.mesh != MeshType::Terrain &&
            !visibilityMap->exploredBounds(o.center, o.half + V3{2, 0, 2})) {
            ++stats.fogRejected;
            return;
        }
        bool needsDistance =
            lod && !camera.strategy &&
            (o.lodClass == 2 || (o.lodClass == 1 && !(layers && o.mesh == MeshType::Leaves)));
        float distance = needsDistance ? length(o.center - cameraEye) : 0.f;
        if (lod && o.lodClass == 3 && length(o.center - cameraEye) > 46) {
            ++stats.distanceSkipped;
            return;
        }
        // A radius around an elevated strategy eye cuts the ground unevenly
        // (or removes all grass when zoomed out). Resident chunks bound cost;
        // use the view frustum for grass coverage in this camera mode.
        if (lod && !camera.strategy && o.lodClass == 2 && distance > 55) {
            ++stats.distanceSkipped;
            return;
        }
        if (culling && !frustum.visible(o.center, o.half))
            return;
        MeshType mesh = o.mesh;
        if (terrainLOD && mesh == MeshType::Terrain &&
            useCoarseTerrain(camera, float(height), o.center, o.half)) {
            mesh = MeshType::TerrainCoarse;
            ++stats.terrainCoarse;
        }
        if (layers && o.mesh == MeshType::Leaves) {
            const auto &selection =
                canopyCache.get(size_t(&o - scene.objects.data()), o.center, o.gpu.model);
            const auto &weights = selection.weights;
            float lower = 0;
            auto emit = [&](MeshType tier, float weight, int atlas = -1) {
                if (weight <= .00001f)
                    return;
                Instance instance = o.gpu;
                instance.surface.y = lower;
                instance.color.w = std::min(1.f, lower + weight);
                lower += weight;
                if (atlas >= 0) {
                    instance.surface.z = 10;
                    instance.surface.w = float(atlas);
                }
                visible[static_cast<int>(tier)].push_back(instance);
            };
            if (weights[0] > .00001f) {
                emit(MeshType::LeavesDetail, weights[0]);
                ++stats.canopyNear;
            }
            if (weights[1] > .00001f) {
                emit(MeshType::Leaves, weights[1]);
                ++stats.canopyMid;
            }
            if (weights[2] > .00001f) {
                for (auto v : selection.views)
                    emit(MeshType::CanopyImpostor, weights[2] * v.weight, v.index);
                ++stats.canopyFar;
            }
            return;
        }
        // Keep the strategy canopy stable while orbiting/zooming.
        // A projected-size transition can replace this fixed near tier later.
        if (lod && !camera.strategy && o.lodClass == 1 && distance > 60) {
            mesh = o.mesh == MeshType::Leaves ? MeshType::LeavesFar : MeshType::FoliageFar;
            ++stats.lodFar;
        }
        const MeshType fullMesh=mesh;
        const MeshType farVegetation=vegetationFarMesh(fullMesh);
        const MeshType midVegetation=vegetationMiddleMesh(fullMesh);
        Instance instance=o.gpu;
        if(lod&&surfaceTextures[0]&&fullMesh!=farVegetation){
            const float pixels=canopyPixels(camera,float(height),o.center,o.half*2);
            if(midVegetation==fullMesh){
                if(pixels<9)mesh=farVegetation;
            }else if(pixels<64){
                const bool detailedRange=pixels>=31;
                const MeshType incoming=detailedRange?fullMesh:midVegetation;
                mesh=detailedRange?midVegetation:farVegetation;
                const float weight=detailedRange?std::clamp((pixels-38.f)/26.f,0.f,1.f)
                                       :std::clamp((pixels-12.f)/19.f,0.f,1.f);
                if(weight>0&&weight<1){
                    auto transition=instance;
                    transition.color.w=weight;
                    transition.surface.z=foliageMaterialForMesh(incoming,transition.surface.z);
                    visible[static_cast<int>(incoming)].push_back(transition);
                    instance.color.w=-weight;
                }
            }
            if(mesh!=fullMesh)++stats.lodFar;
        }
        instance.surface.z=foliageMaterialForMesh(mesh,instance.surface.z);
        if (materials && o.mesh == MeshType::Branch && instance.surface.z == 5 && instance.surface.w == 0)
            instance.surface.w = -1;
        if (materials && scene.materialShowcase && o.mesh == MeshType::Rock)
            mesh = MeshType::RockDetail;
        visible[static_cast<int>(mesh)].push_back(instance);
    };
    size_t cursor = 0;
    if (culling && regionCulling) {
        for (const auto &region : scene.regions) {
            for (; cursor < region.begin; ++cursor)
                visit(scene.objects[cursor]);
            ++stats.regionTests;
            if (!frustum.visible(region.center, region.half)) {
                ++stats.regionsRejected;
                stats.objectsRegionSkipped += static_cast<int>(region.end - region.begin);
                cursor = region.end;
            } else if (visibilityMap && visibilityCulling &&
                       !visibilityMap->exploredBounds(region.center, region.half + V3{2, 0, 2})) {
                // Unexplored region: preserve terrain, bypass per-object map queries.
                for (; cursor < region.end; ++cursor) {
                    if (scene.objects[cursor].mesh == MeshType::Terrain)
                        visit(scene.objects[cursor]);
                    else {
                        ++stats.objectCandidates;
                        ++stats.fogRejected;
                    }
                }
            } else {
                for (; cursor < region.end; ++cursor)
                    visit(scene.objects[cursor]);
            }
        }
    }
    // Moving actors remain outside static regions and are always tested individually.
    for (; cursor < scene.objects.size(); ++cursor)
        visit(scene.objects[cursor]);
    stats.cullMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (layers) {
        stats.canopyCacheHits = canopyCache.hits;
        stats.canopyCacheMisses = canopyCache.misses;
    }
    stats.canopyCacheBytes = canopyCache.bytes();
    auto sceneBegin = stamp();
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.Viewport(0, 0, width, height);
    gl.ClearColor(0, 0, 0, 0);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.UseProgram(sceneProgram);
    gl.Uniform1i(naturalLoc, natural ? 1 : 0);
    gl.Uniform1i(materialsLoc, materials ? 1 : 0);
    gl.Uniform1i(wetLoc, water ? 1 : 0);
    gl.Uniform1f(wetTimeLoc, float(scene.time));
    gl.Uniform1f(waveLoc, std::clamp(waveStrength,0.f,1.f));
    for (int i = 0; i < 6; ++i)
        bindTexture(materialTextures[i], 5 + i);
    gl.Uniform1f(sceneTime, float(scene.time));
    gl.Uniform1f(fogDensity, scene.visualReference ? .0025f : scene.dynamicWorld ? .012f : .003f);
    gl.UniformMatrix4fv(loc.vp, 1, GL_FALSE, vp.v.data());
    gl.UniformMatrix4fv(loc.light, 1, GL_FALSE, lightVP.v.data());
    gl.UniformMatrix4fv(loc.view, 1, GL_FALSE, view.v.data());
    auto eye = camera.eye();
    gl.Uniform3f(loc.eye, eye.x, eye.y, eye.z);
    gl.Uniform1i(visibilityEnabled, visibilityMap ? 1 : 0);
    if (visibilityMap) {
        bindTexture(visibilityTexture, 3);
        gl.Uniform1i(visibilitySampler, 3);
    }
    bindTexture(shadowTexture, 0);
    gl.Uniform1i(loc.shadow, 0);
    gl.Uniform1i(loc.enabled, shadows ? 1 : 0);
    for (int i = 0; i < meshCount; ++i) {
        if (i == static_cast<int>(MeshType::CanopyImpostor) && !visible[i].empty()) {
            gl.UseProgram(impostorProgram);
            gl.UniformMatrix4fv(impostorLoc[0], 1, GL_FALSE, vp.v.data());
            gl.UniformMatrix4fv(impostorLoc[1], 1, GL_FALSE, lightVP.v.data());
            gl.UniformMatrix4fv(impostorLoc[2], 1, GL_FALSE, view.v.data());
            gl.Uniform3f(impostorLoc[3], eye.x, eye.y, eye.z);
            gl.Uniform1i(impostorLoc[4], shadows ? 1 : 0);
            gl.Uniform1i(impostorLoc[5], 0);
            gl.Uniform1i(impostorLoc[6], visibilityMap ? 1 : 0);
            gl.Uniform1i(impostorLoc[7], 3);
            gl.Uniform1f(impostorLoc[8], scene.visualReference ? .0025f
                                         : scene.dynamicWorld  ? .012f
                                                               : .003f);
            gl.Uniform1i(impostorLoc[9], natural ? 1 : 0);
            gl.Uniform1f(impostorLoc[10], float(scene.time));
            bindTexture(canopyTexture, 4);
            gl.Uniform1i(impostorLoc[11], 4);
        }
        drawMesh(meshes[i], visible[i], traceRender ? &stats.trace.sceneUpload : nullptr,
                 traceRender ? &stats.trace.sceneDraw : nullptr);
        stats.visible += static_cast<int>(visible[i].size());
        stats.triangles += visible[i].size() * (meshes[i].indices / 3);
        if (!visible[i].empty())
            ++stats.sceneDraws;
    }
    auto sceneEnd = stamp();
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.UseProgram(compositeProgram);
    bindTexture(targets[0], 0);
    bindTexture(targets[1], 1);
    bindTexture(targets[3], 2);
    bindTexture(targets[2], 3);
    gl.Uniform1i(contactLoc, natural ? 1 : 0);
    gl.Uniform1i(waterLoc[0], water ? 1 : 0);
    gl.Uniform1i(waterLoc[6], waterSSR ? 1 : 0);
    gl.Uniform1f(waterLoc[1], float(scene.time));
    gl.Uniform1f(waterLoc[7], std::clamp(waveStrength,0.f,1.f));
    gl.Uniform3f(waterLoc[2], eye.x, eye.y, eye.z);
    gl.Uniform3f(waterLoc[3], view.at(0, 0), view.at(0, 1), view.at(0, 2));
    gl.Uniform3f(waterLoc[4], view.at(1, 0), view.at(1, 1), view.at(1, 2));
    gl.Uniform3f(waterLoc[5], -view.at(2, 0), -view.at(2, 1), -view.at(2, 2));
    auto projection = camera.projection(float(width) / height);
    gl.Uniform2f(projectionLoc, projection.at(0, 0), projection.at(1, 1));
    gl.Uniform1f(exposureLoc, exposure);
    gl.Uniform1i(aaLoc, aa ? 1 : 0);
    gl.BindVertexArray(emptyVAO);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    auto renderEnd = stamp();
    stats.trace.sceneSubmit = renderMs(sceneBegin, sceneEnd);
    stats.trace.composite = renderMs(sceneEnd, renderEnd);
    stats.trace.total = renderMs(renderBegin, renderEnd);
    return stats;
}
void Renderer::drawUI(const UI &ui) {
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.UseProgram(uiProgram);
    gl.Uniform2f(uiSize, float(width), float(height));
    gl.BindVertexArray(uiVAO);
    gl.BindBuffer(GL_ARRAY_BUFFER, uiVBO);
    gl.BufferData(GL_ARRAY_BUFFER, ui.vertices.size() * sizeof(UIVertex), ui.vertices.data(),
                  GL_STREAM_DRAW);
    gl.DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(ui.vertices.size()));
    gl.Disable(GL_BLEND);
}
void Renderer::capture(const std::string &file) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3), flipped(pixels.size());
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl.ReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y < height; ++y)
        std::copy_n(pixels.data() + size_t(height - 1 - y) * width * 3, width * 3,
                    flipped.data() + size_t(y) * width * 3);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(flipped.data(), width, height, 24, width * 3,
                                                        SDL_PIXELFORMAT_RGB24);
    if (!s)
        throw std::runtime_error(SDL_GetError());
    int result = SDL_SaveBMP(s, file.c_str());
    SDL_FreeSurface(s);
    if (result != 0)
        throw std::runtime_error("Cannot save capture: " + file);
    gl.check("Capture");
}
void Renderer::beginGPU(int frame) {
    queryActive = false;
    if (!gpuTimingEnabled)
        return;
    for (auto &q : queries)
        if (q.frame < 0) {
            q.frame = frame;
            gl.BeginQuery(GL_TIME_ELAPSED, q.id);
            queryActive = true;
            break;
        }
}
void Renderer::endGPU() {
    if (queryActive)
        gl.EndQuery(GL_TIME_ELAPSED);
    queryActive = false;
}
std::vector<std::pair<int, double>> Renderer::collectGPU(bool wait) {
    std::vector<std::pair<int, double>> out;
    if (!gpuTimingEnabled)
        return out;
    for (auto &q : queries)
        if (q.frame >= 0) {
            GLint ready = 0;
            gl.GetQueryObjectiv(q.id, GL_QUERY_RESULT_AVAILABLE, &ready);
            if (ready || wait) {
                GLuint64 ns = 0;
                gl.GetQueryObjectui64v(q.id, GL_QUERY_RESULT, &ns);
                out.emplace_back(q.frame, ns / 1e6);
                q.frame = -1;
            }
        }
    return out;
}
void Renderer::setVisibility(const VisibilityMap *map, bool cull) {
    visibilityMap = map;
    visibilityCulling = cull;
    if (!map)
        return;
    if (!visibilityTexture) {
        gl.GenTextures(1, &visibilityTexture);
        bindTexture(visibilityTexture, 3);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RG8, VisibilityMap::size, VisibilityMap::size, 0, GL_RG,
                      GL_UNSIGNED_BYTE, nullptr);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if (visibilityRevision != map->revision) {
        bindTexture(visibilityTexture, 3);
        gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VisibilityMap::size, VisibilityMap::size, GL_RG,
                         GL_UNSIGNED_BYTE, map->pixels.data());
        visibilityRevision = map->revision;
    }
}
double Renderer::targetMiB() const {
    return (double(width) * height * 28 + double(shadowSize) * shadowSize * 4 +
            (visibilityTexture ? VisibilityMap::size * VisibilityMap::size * 2 : 0)) /
           (1024 * 1024);
}
} // namespace pl
