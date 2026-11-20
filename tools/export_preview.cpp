#define SDL_MAIN_HANDLED
#include "construction.hpp"
#include "renderer.hpp"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace pl {
struct PreviewExporter {
    static void write(Renderer &renderer, const std::string &directory) {
        auto getSize = reinterpret_cast<PFNGLGETBUFFERPARAMETERIVPROC>(SDL_GL_GetProcAddress("glGetBufferParameteriv"));
        auto getData = reinterpret_cast<PFNGLGETBUFFERSUBDATAPROC>(SDL_GL_GetProcAddress("glGetBufferSubData"));
        if (!getSize || !getData) throw std::runtime_error("OpenGL buffer readback is unavailable");
        std::filesystem::create_directories(directory);
        std::ofstream meta(directory + "/scene.json");
        meta << "{\"vertexStride\":" << sizeof(Vertex) << ",\"meshes\":[";
        for (int i = 0; i < meshCount; ++i) {
            const auto &mesh = renderer.meshes[i];
            renderer.gl.BindVertexArray(mesh.vao);
            auto dump = [&](GLenum target, GLuint buffer, const char *extension) {
                renderer.gl.BindBuffer(target, buffer);
                GLint count = 0;
                getSize(target, GL_BUFFER_SIZE, &count);
                std::vector<char> data(count);
                getData(target, 0, count, data.data());
                std::ofstream stream(directory + "/mesh-" + std::to_string(i) + extension, std::ios::binary);
                if (!stream.write(data.data(), count)) throw std::runtime_error("Cannot export mesh data");
            };
            dump(GL_ARRAY_BUFFER, mesh.vbo, ".vbo");
            dump(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo, ".ibo");
            if (i) meta << ',';
            meta << "{\"id\":" << i << ",\"indices\":" << mesh.indices;
            const auto far = vegetationFarMesh(static_cast<MeshType>(i));
            if (int(far) != i) {
                meta << ",\"far\":" << int(far);
                float material = foliageMaterialForMesh(far, -1);
                if (material >= 0) meta << ",\"farMaterial\":" << material;
            }
            const auto mid = vegetationMiddleMesh(static_cast<MeshType>(i));
            if (int(mid) != i) meta << ",\"mid\":" << int(mid);
            meta << '}';
        }
        auto scene = [&](const Scene &source) {
            meta << '[';
            bool first = true;
            for (const auto &object : source.objects) {
                if (!first) meta << ',';
                first = false;
                const auto mesh = object.mesh == MeshType::Rock ? MeshType::RockDetail : object.mesh;
                Instance instance = object.gpu;
                if (mesh == MeshType::Branch && instance.surface.z == 5 && instance.surface.w == 0)
                    instance.surface.w = -1;
                meta << "{\"mesh\":" << int(mesh);
                if (object.vegetation != VegetationSection::None)
                    meta << ",\"section\":" << int(object.vegetation) << ",\"parentDistance\":" << object.parentDistance;
                if (!object.visible) meta << ",\"visible\":false";
                if (object.lodClass == 3) meta << ",\"detail\":true";
                meta << ",\"instance\":[";
                auto component = [&](float value) { meta << ',' << value; };
                meta << instance.model.v[0];
                for (int i = 1; i < 16; ++i) component(instance.model.v[i]);
                for (float value : {instance.color.x, instance.color.y, instance.color.z, instance.color.w,
                                    instance.surface.x, instance.surface.y, instance.surface.z, instance.surface.w})
                    component(value);
                meta << "]}";
            }
            meta << ']';
        };
        meta << "],\"shore\":";
        scene(makeShoreScene());
        meta << ",\"building\":";
        scene(makeBuildingScene());
        meta << ",\"buildingForms\":{\"stone\":";
        scene(makeBuildingScene(BuildingForm::Stone));
        meta << ",\"shelter\":";
        scene(makeBuildingScene(BuildingForm::Shelter));
        meta << '}';
        meta << ",\"meshIds\":{\"rock\":" << int(MeshType::RockDetail) << ",\"box\":" << int(MeshType::BeveledBox)
             << ",\"roof\":" << int(MeshType::Roof) << ",\"vine\":" << int(MeshType::VineStem) << "}";
        meta << ",\"plants\":{";
        auto flat = [](float, float) { return 0.f; };
        for (const auto &[name, species] : {
                 std::pair{"spruce", TreeSpecies::Spruce}, std::pair{"pine", TreeSpecies::ScotsPine},
                 std::pair{"birch", TreeSpecies::Birch}, std::pair{"tropicalTree", TreeSpecies::TropicalBroadleaf}}) {
            Scene specimen(0, false);
            appendTree(specimen.objects, species, {0, 0, 0}, 1.f, 491u, flat);
            meta << '"' << name << "\":";
            scene(specimen);
            meta << ',';
            Scene variant(0, false);
            appendTree(variant.objects, species, {0, 0, 0}, 1.f, 846u, flat);
            meta << '"' << name << "2\":";
            scene(variant);
            meta << ',';
        }
        for (const auto &[name, species] : {
                 std::pair{"fern", GroundPlant::Fern}, std::pair{"juniper", GroundPlant::Juniper}}) {
            Scene specimen(0, false);
            appendGroundPlant(specimen.objects, species, {0, 0, 0}, 1.f, 932u, flat);
            meta << '"' << name << "\":";
            scene(specimen);
            meta << ',';
        }
        Scene bamboo(0, false);
        VegetationRandom random(2607);
        for (int i = 0; i < 4; ++i) {
            const V3 base{random.range(-.14f, .14f), 0, random.range(-.14f, .14f)};
            const V3 stem = unit({random.range(-.19f, .19f), 1, random.range(-.19f, .19f)});
            const float h = random.range(.74f, 1.01f);
            size_t collar = appendVegetationPart(bamboo.objects, MeshType::BambooStem, base + stem * (h * .48f),
                stem, {.020f, h, .020f}, {.91f, 1.f, .87f}, materialPattern(SurfaceLayer::Bamboo),
                VegetationSection::Collar, noVegetationParent);
            for (int twig = 0; twig < 4; ++twig) {
                float azimuth = twig * 2.39996f + i * .91f;
                V3 direction = unit(horizontal(azimuth) + V3{0, .27f, 0});
                V3 shoot = base + stem * (h * (.51f + twig * .11f));
                appendVegetationPart(bamboo.objects, MeshType::BambooLeaves, shoot + direction * .12f,
                    direction, {.22f, .24f, .22f}, {.93f, 1.f, .81f}, materialPattern(SurfaceLayer::Broadleaf),
                    VegetationSection::Foliage, collar, 0, random.range(-1, 1));
            }
        }
        meta << "\"bamboo\":";
        scene(bamboo);
        Scene palm(0, false);
        size_t crown = appendVegetationPart(palm.objects, MeshType::BambooStem, {0, .47f, 0},
            {.11f, 1, -.026f}, {.051f, .96f, .051f}, {.98f, .94f, .91f}, materialPattern(SurfaceLayer::PalmStem),
            VegetationSection::Collar, noVegetationParent);
        for (int leaf = 0; leaf < 10; ++leaf) {
            const bool young = leaf >= 7;
            const V3 direction = unit(horizontal(leaf * 2.39996f + random.range(-.15f, .15f)) +
                                      V3{0, young ? random.range(.45f, .85f) : random.range(-.18f, .20f), 0});
            const V3 across = unit(cross(direction, std::abs(direction.z) < .92f ? V3{0, 0, 1} : V3{0, 1, 0}));
            const V3 normal = cross(across, direction);
            const float roll = std::atan2(normal.y, across.y) + random.range(-.15f, .15f);
            const V3 from{.058f, .944f, -.012f};
            const float span = young ? random.range(.23f, .34f) : random.range(.35f, .48f);
            const float spread = young ? .48f : .72f;
            appendVegetationPart(palm.objects, MeshType::PalmFrond, from + direction * (span * .50f),
                direction, {span * .83f, span, span * spread}, {.86f, .99f, .80f}, materialPattern(SurfaceLayer::Broadleaf),
                VegetationSection::Foliage, crown, 0, roll);
        }
        meta << ",\"palm\":";
        scene(palm);
        meta << "},\"snowCabin\":";
        Scene cabin(0, false);
        appendCabin(cabin, true);
        scene(cabin);
        meta << "}\n";
        if (!meta) throw std::runtime_error("Cannot export scene metadata");
    }
};
} // namespace pl

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: pleiade-export-preview MATERIAL_DIR WEB_ASSETS_DIR\n";
        return 2;
    }
    SDL_SetMainReady();
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << SDL_GetError(); return 3; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window = SDL_CreateWindow("export", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    auto context = window ? SDL_GL_CreateContext(window) : nullptr;
    if (!context) { std::cerr << SDL_GetError(); SDL_Quit(); return 3; }
    int result = 0;
    try {
        pl::Renderer renderer(64, 64, 512, false, argv[1]);
        pl::PreviewExporter::write(renderer, argv[2]);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 4;
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
