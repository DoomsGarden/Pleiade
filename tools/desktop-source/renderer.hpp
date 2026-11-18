#pragma once
#include "canopy_cache.hpp"
#include "gl_api.hpp"
#include "scene.hpp"
#include "ui.hpp"
#include <array>
#include <string>

namespace pl {
struct VisibilityMap;
struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0, instances = 0;
    GLsizei indices = 0;
};
struct RenderTrace {
    double total = 0, state = 0, shadowPrepare = 0, shadowSubmit = 0;
    double sceneSubmit = 0, composite = 0;
    double shadowUpload = 0, shadowDraw = 0, sceneUpload = 0, sceneDraw = 0;
};
struct RenderStats {
    RenderTrace trace{};
    int terrainCoarse = 0;
    int canopyNear = 0, canopyMid = 0, canopyFar = 0;
    int canopyCacheHits = 0, canopyCacheMisses = 0;
    size_t canopyCacheBytes = 0;

    int visible = 0, total = 0, sceneDraws = 0, fogRejected = 0;
    size_t triangles = 0;
    double cullMs = 0;
    int lodFar = 0, distanceSkipped = 0, shadowDraws = 0;
    int regionTests = 0, regionsRejected = 0, objectCandidates = 0, objectsRegionSkipped = 0;
};
struct QuerySlot {
    GLuint id = 0;
    int frame = -1;
};
class Renderer {
    friend struct PreviewExporter;
  public:
    GL gl;
    int width = 0, height = 0, shadowSize = 2048;
    std::string vendor, gpu, api;
    explicit Renderer(int w, int h, int shadowResolution, bool gpuTiming = true,
                      const std::string &assetDirectory = "");
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void resize(int w, int h);
    RenderStats render(const Scene &, const Camera &, bool culling, bool shadows, float exposure,
                       bool lod = true, bool aa = false, bool regionCulling = true,
                       bool traceRender = false, bool terrainLOD = false, bool natural = false,
                       bool layers = false, bool materials = false, bool water = false,
                       bool waterSSR = true, float waveStrength = 1.0f);
    void drawUI(const UI &ui);
    void capture(const std::string &file);
    void beginGPU(int frame);
    void endGPU();
    std::vector<std::pair<int, double>> collectGPU(bool wait = false);
    double targetMiB() const;
    void setVisibility(const VisibilityMap *map, bool cull = true);

  private:
    CanopyCache canopyCache;
    std::array<Mesh, meshCount> meshes{};
    GLuint sceneProgram = 0, shadowProgram = 0, compositeProgram = 0, uiProgram = 0;
    GLuint fbo = 0, shadowFbo = 0, shadowTexture = 0, emptyVAO = 0, uiVAO = 0, uiVBO = 0;
    std::array<GLuint, 4> targets{};
    std::array<std::vector<Instance>, meshCount> visible;
    std::array<QuerySlot, 16> queries{};
    bool queryActive = false, shadowValid = false;
    bool gpuTimingEnabled = true;
    M4 lightVP;
    const VisibilityMap *visibilityMap = nullptr;
    bool visibilityCulling = true;
    GLuint visibilityTexture = 0;
    std::array<GLuint, 6> materialTextures{};
    std::array<GLuint, 2> surfaceTextures{};
    GLint materialsLoc = 0, wetLoc = 0, wetTimeLoc = 0, waveLoc = 0;
    std::array<GLint, 8> waterLoc{};
    GLuint canopyTexture = 0, impostorProgram = 0;
    std::array<GLint, 12> impostorLoc{};
    uint64_t visibilityRevision = ~uint64_t(0);
    GLint visibilityEnabled = 0, visibilitySampler = 0;
    struct Locations {
        GLint vp, eye, light, view, shadow, enabled;
    } loc{};
    GLint shadowVP = 0, exposureLoc = 0, uiSize = 0;
    GLint naturalLoc = 0, contactLoc = 0, projectionLoc = 0;
    GLint sceneTime = 0, shadowTime = 0, fogDensity = 0, aaLoc = 0;
    GLuint program(const char *, const char *);
    Mesh makeMesh(const std::vector<Vertex> &, const std::vector<uint32_t> &);
    void drawMesh(const Mesh &, const std::vector<Instance> &, double *uploadMs = nullptr,
                  double *drawMs = nullptr);
    void createShadow();
    void bindTexture(GLuint texture, int unit);
};
} // namespace pl
