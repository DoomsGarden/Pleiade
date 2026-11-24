#define SDL_MAIN_HANDLED
#include "benchmark.hpp"
#include "cinematic_camera.hpp"
#include "pacing.hpp"
#include "player_controller.hpp"
#include "renderer.hpp"
#include "scout_group.hpp"
#include "shore.hpp"
#include "simulation.hpp"
#include "strategy_selection.hpp"
#include "version.hpp"
#include "world.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <thread>

namespace pl {
using Clock = std::chrono::steady_clock;
double milliseconds(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}
struct Options {
    int width = 1600, height = 900, frames = 0, warmup = 0, stress = 0, shadowSize = 2048;
    int strategyDistance = 0;
    int seconds = 0, warmupSeconds = 5, maxFPS = 60;
    bool world = false, reference = false, aa = true, lod = true, freeze = false;
    bool walkDemo = false, cinematic = false;
    double cinematicTime = 0;
    int inspectionAngle = 0;
    bool inspectTree = false;
    bool materials = false, shore = false, water = true, waterSSR = true;
    int waterTime = 0, waveStrength = 100;
    std::string assetDirectory;
    bool terrainLOD = false, fixedCamera = false, natural = false, layers = false;
    bool fowDemo = false, fowCulling = true;
    int visionRadius = 36;
    bool legacyPacing = false, traceRender = false, gpuTiming = true;
    bool titleUpdates = true, regionCulling = true, frameGlChecks = false;
    int seed = 12345, worldRadius = 3, actors = 24, chunkBudget = 2;
    bool benchmark = false, headless = false, culling = true, shadows = true, hud = true,
         strategy = false, resizeTest = false;
    std::string output = "results", capture;
};
Options parse(int argc, char **argv) {
    Options o;
    bool explicitCinematicTime = false;
    bool explicitFrames = false, explicitWarmup = false, explicitSeconds = false,
         explicitWarmupSeconds = false, explicitMaxFPS = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto value = [&]() {
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing value for " + a);
            return std::string(argv[++i]);
        };
        auto number = [&](int lo, int hi) {
            auto s = value();
            size_t n = 0;
            int x = std::stoi(s, &n);
            if (n != s.size() || x < lo || x > hi)
                throw std::invalid_argument("Invalid value for " + a);
            return x;
        };
        if (a == "--scene") {
            auto name = value();
            if (name != "courtyard" && name != "world" && name != "reference")
                throw std::invalid_argument("Scene must be courtyard, world or reference");
            o.world = name != "courtyard";
            o.reference = name == "reference";
        } else if (a == "--seed")
            o.seed = number(0, 2147483647);
        else if (a == "--world-radius")
            o.worldRadius = number(1, 4);
        else if (a == "--materials")
            o.materials = true;
        else if (a == "--water-demo")
            o.shore = true;
        else if (a == "--walk-demo")
            o.walkDemo = true;
        else if (a == "--cinematic")
            o.cinematic = true;
        else if (a == "--cinematic-time") {
            auto raw = value();
            size_t parsed = 0;
            o.cinematicTime = std::stod(raw, &parsed);
            if (parsed != raw.size() || !std::isfinite(o.cinematicTime) ||
                o.cinematicTime < 0 || o.cinematicTime > 3600)
                throw std::invalid_argument("Invalid value for " + a);
            explicitCinematicTime = true;
        } else if (a == "--no-water-ssr")
            o.waterSSR = false;
        else if (a == "--wave-strength")
            o.waveStrength = number(0, 100);
        else if (a == "--water-time")
            o.waterTime = number(0, 3600);
        else if (a == "--no-water")
            o.water = false;
        else if (a == "--asset-dir")
            o.assetDirectory = value();
        else if (a == "--inspect-tree") {
            o.inspectTree = true;
            o.world = true;
            o.reference = true;
            o.fixedCamera = true;
        } else if (a == "--inspection-angle")
            o.inspectionAngle = number(0, 359);
        else if (a == "--strategy-distance")
            o.strategyDistance = number(8, 100);
        else if (a == "--actors")
            o.actors = number(0, 512);
        else if (a == "--chunk-budget")
            o.chunkBudget = number(1, 8);
        else if (a == "--no-aa")
            o.aa = false;
        else if (a == "--no-lod")
            o.lod = false;
        else if (a == "--freeze")
            o.freeze = true;
        else if (a == "--width")
            o.width = number(320, 7680);
        else if (a == "--height")
            o.height = number(240, 4320);
        else if (a == "--frames") {
            o.frames = number(1, 100000);
            explicitFrames = true;
        } else if (a == "--warmup") {
            o.warmup = number(0, 10000);
            explicitWarmup = true;
        } else if (a == "--seconds") {
            o.seconds = number(1, 3600);
            explicitSeconds = true;
        } else if (a == "--warmup-seconds") {
            o.warmupSeconds = number(0, 600);
            explicitWarmupSeconds = true;
        } else if (a == "--max-fps") {
            o.maxFPS = number(0, 1000);
            explicitMaxFPS = true;
        } else if (a == "--stress")
            o.stress = number(0, 10000);
        else if (a == "--shadow-size") {
            o.shadowSize = number(512, 4096);
            if (o.shadowSize & (o.shadowSize - 1))
                throw std::invalid_argument("Shadow size must be a power of two");
        } else if (a == "--benchmark")
            o.benchmark = true;
        else if (a == "--headless")
            o.headless = true;
        else if (a == "--no-culling")
            o.culling = false;
        else if (a == "--no-shadows")
            o.shadows = false;
        else if (a == "--fixed-camera")
            o.fixedCamera = true;
        else if (a == "--layers")
            o.layers = true;
        else if (a == "--natural")
            o.natural = true;
        else if (a == "--terrain-lod")
            o.terrainLOD = true;
        else if (a == "--fow-demo") {
            o.fowDemo = true;
            o.world = true;
            o.reference = true;
            o.strategy = true;
        } else if (a == "--no-fow-culling")
            o.fowCulling = false;
        else if (a == "--vision-radius")
            o.visionRadius = number(8, 64);
        else if (a == "--no-gpu-timing")
            o.gpuTiming = false;
        else if (a == "--trace-render")
            o.traceRender = true;
        else if (a == "--legacy-pacing")
            o.legacyPacing = true;
        else if (a == "--frame-gl-checks")
            o.frameGlChecks = true;
        else if (a == "--no-region-culling")
            o.regionCulling = false;
        else if (a == "--no-title-updates")
            o.titleUpdates = false;
        else if (a == "--no-hud")
            o.hud = false;
        else if (a == "--resize-test")
            o.resizeTest = true;
        else if (a == "--output")
            o.output = value();
        else if (a == "--capture")
            o.capture = value();
        else if (a == "--camera") {
            auto c = value();
            if (c != "fps" && c != "strategy")
                throw std::invalid_argument("Camera must be fps or strategy");
            o.strategy = c == "strategy";
        } else if (a == "--help") {
            std::cout
                << "Pleiade " << version
                << "\n--water-demo --no-water --no-water-ssr --water-time 0..3600 --wave-strength 0..100 --materials "
                   "--asset-dir DIRECTORY\n--layers (F8 canopy "
                   "layers)\n--natural (F7 toggles materials + ambient "
                   "contact)\n--scene "
                   "courtyard|world|reference --seed N --world-radius 1..4 --actors "
                   "0..512\n--inspect-tree --inspection-angle 0..359 (fixed view) --strategy-distance "
                   "8..100 "
                   "--chunk-budget 1..8 --no-lod --no-aa "
                   "--freeze\n--benchmark [--seconds N "
                   "--warmup-seconds N | --frames N --warmup N]\n--max-fps N "
                   "(0 unlimited; interactive default 60)\n--camera fps|strategy --walk-demo\n"
                   "--cinematic [--cinematic-time SECONDS]\n--width N --height N "
                   "--stress 0..10000 --shadow-size 512|1024|2048|4096\n--no-culling --no-shadows "
                   "--no-hud --headless --resize-test --no-title-updates --no-region-culling "
                   "--fixed-camera --terrain-lod --fow-demo --no-fow-culling --vision-radius "
                   "8..64\n--no-gpu-timing "
                   "--frame-gl-checks --trace-render --legacy-pacing\n--output "
                   "DIRECTORY --capture "
                   "FILE.bmp\n";
            std::exit(0);
        } else
            throw std::invalid_argument("Unknown argument: " + a);
    }
    if (explicitCinematicTime && !o.cinematic)
        throw std::invalid_argument("--cinematic-time requires --cinematic");
    if (o.cinematic && (o.benchmark || o.fowDemo || o.inspectTree || o.walkDemo || o.resizeTest))
        throw std::invalid_argument("--cinematic needs an independent camera");
    if (o.walkDemo && (o.benchmark || o.fowDemo || o.inspectTree))
        throw std::invalid_argument("--walk-demo needs an interactive player camera");
    if ((explicitSeconds || explicitWarmupSeconds) && !o.benchmark)
        throw std::invalid_argument("Time options require --benchmark");
    if (explicitFrames && (explicitSeconds || explicitWarmupSeconds))
        throw std::invalid_argument("Choose frames OR seconds, not both");
    if (o.benchmark) {
        if (!explicitFrames) {
            if (explicitWarmup)
                throw std::invalid_argument("Timed mode uses --warmup-seconds");
            if (!explicitSeconds)
                o.seconds = 30;
        } else if (!explicitWarmup)
            o.warmup = 120;
        if (!explicitMaxFPS)
            o.maxFPS = 0;
    } else if (o.headless && !explicitMaxFPS)
        o.maxFPS = 0;
    if (o.world && o.stress)
        throw std::invalid_argument("Use --actors for world; --stress belongs to courtyard");
    if (o.seconds && !o.capture.empty())
        throw std::invalid_argument("Use frame mode for captures; timed benchmarks avoid readback");
    if (o.headless && o.frames == 0 && o.seconds == 0)
        throw std::invalid_argument("Headless mode needs --frames or --benchmark");
    if (o.fowDemo) {
        o.world = true;
        o.reference = true;
        o.strategy = true;
        o.actors = 0;
    }
    if (o.shore) {
        o.world = false;
        o.reference = true;
        o.fixedCamera = true;
        o.materials = true;
        o.natural = true;
    }
    return o;
}
struct Platform {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    explicit Platform(const Options &o) {
#ifndef _WIN32
        if (o.headless)
            SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
#endif
        SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        std::string title = std::string("PLEIADE ") + version + " | TAB: camera | F1: help";
        window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, o.width,
                                  o.height,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                      (o.headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
        if (!window)
            throw std::runtime_error(SDL_GetError());
        context = SDL_GL_CreateContext(window);
        if (!context)
            throw std::runtime_error(std::string("OpenGL 4.3 required: ") + SDL_GetError());
        SDL_GL_SetSwapInterval(o.benchmark || o.headless ? 0 : 1);
    }
    ~Platform() {
        if (context)
            SDL_GL_DeleteContext(context);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }
};
struct Sample {
    double frame = 0, cpu = 0, gpu = -1, cull = 0;
    int visible = 0, draws = 0;
    size_t triangles = 0;
    double work = 0, pacing = 0;
    bool measured = true;
    WorldStats world;
    int lodFar = 0, distanceSkipped = 0, shadowDraws = 0, simSteps = 0;
    uint64_t droppedSteps = 0;
    double runSeconds = 0, measuredSeconds = 0, startInterval = 0, loopGap = 0;
    double events = 0, update = 0, queries = 0, glCheck = 0, swap = 0, other = 0;
    double post = 0, title = 0;
    bool titleUpdated = false, storageGrew = false;
    int regionTests = 0, regionsRejected = 0, objectCandidates = 0, objectsRegionSkipped = 0;
    RenderTrace trace{};
    double queryBegin = 0, queryEnd = 0, hud = 0;
    int terrainCoarse = 0;
    int canopyNear = 0, canopyMid = 0, canopyFar = 0;
    int canopyCacheHits = 0, canopyCacheMisses = 0;
    size_t canopyCacheBytes = 0;
    int fogVisible = 0, fogExplored = 0, fogRejected = 0;
    double fogUpdate = 0;
    V3 cameraEye{}, cameraTarget{};
};
std::string quote(const std::string &s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c == '\n')
            out += "\\n";
        else if (c < 32)
            out += ' ';
        else
            out += char(c);
    }
    return out + '"';
}
std::string fixed(double v, int precision = 2) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(precision) << v;
    return s.str();
}
void saveReport(const Options &o, const Renderer &r, const std::vector<Sample> &all, int completed,
                int resizeCount, int toggleCount, bool complete, double wallSeconds,
                double actualWarmupSeconds, const FramePacer &pacer, const VisibilityMap *fog) {
    std::filesystem::create_directories(o.output);
    std::ofstream csv(std::filesystem::path(o.output) / "frames.csv");
    if (!csv)
        throw std::runtime_error("Cannot write frames.csv");
    const std::string header =
        "frame,frame_ms,cpu_submit_ms,gpu_render_ms,cull_ms,visible,scene_draw_calls,triangles,work_"
        "frame_ms,pacing_ms,loaded_chunks,pending_chunks,generated_chunks,unloaded_chunks,world_"
        "update_ms,lod_far,grass_distance_skipped,shadow_draw_calls,simulation_steps,dropped_"
        "simulation_steps,run_frame,run_elapsed_seconds,measured_elapsed_seconds,frame_start_interval_"
        "ms,"
        "loop_gap_ms,events_window_ms,update_total_ms,gpu_query_collect_ms,gl_check_ms,swap_ms,"
        "other_cpu_ms,post_frame_ms,title_update_ms,title_updated,sample_storage_grew,cycle_ms,active_"
        "cycle_ms,region_tests,regions_rejected,object_candidates,objects_region_skipped,deadline_"
        "lateness_ms,cpu_render_total_ms,cpu_render_state_ms,cpu_shadow_prepare_ms,cpu_shadow_submit_ms,"
        "cpu_scene_submit_ms,cpu_composite_ms,cpu_shadow_upload_ms,cpu_shadow_draw_ms,"
        "cpu_scene_upload_ms,cpu_scene_draw_ms,cpu_query_begin_ms,cpu_query_end_ms,cpu_hud_ms,fow_"
        "visible_cells,fow_explored_cells,fow_rejected_objects,fow_update_ms,camera_eye_x,camera_eye_y,"
        "camera_eye_z,camera_target_x,camera_target_y,camera_target_z,terrain_coarse_chunks,canopy_near,"
        "canopy_mid,canopy_far,canopy_cache_hits,canopy_cache_misses,canopy_cache_bytes\n";
    csv << header;
    std::ofstream spikes(std::filesystem::path(o.output) / "spikes.csv");
    if (!spikes)
        throw std::runtime_error("Cannot write spikes.csv");
    spikes << header;
    int spikeCount = 0;
    double maxCycle = 0, maxActive = 0, maxGap = 0, maxLateness = 0;

    csv << std::fixed << std::setprecision(6);
    std::vector<double> times, gpuTimes, cpuTimes, workTimes, pacingTimes;
    int warmupFrames = 0, row = 0;
    for (int i = 0; i < completed; ++i) {
        const auto &s = all.at(i);
        if (!s.measured) {
            ++warmupFrames;
            continue;
        }
        workTimes.push_back(s.work);
        pacingTimes.push_back(s.pacing);
        double lateness = o.maxFPS > 0 ? std::max(0.0, s.frame + s.post - 1000.0 / o.maxFPS) : 0;
        maxLateness = std::max(maxLateness, lateness);
        std::ostringstream line;
        line << std::fixed << std::setprecision(6);
        line << row++ << ',' << s.frame << ',' << s.cpu << ',';
        if (s.gpu >= 0)
            line << s.gpu;
        line << ',' << s.cull << ',' << s.visible << ',' << s.draws << ',' << s.triangles << ','
             << s.work << ',' << s.pacing << ',' << s.world.chunks << ',' << s.world.pending << ','
             << s.world.generated << ',' << s.world.unloaded << ',' << s.world.generationMs << ','
             << s.lodFar << ',' << s.distanceSkipped << ',' << s.shadowDraws << ',' << s.simSteps << ','
             << s.droppedSteps << ',' << i << ',' << s.runSeconds << ',' << s.measuredSeconds << ','
             << s.startInterval << ',' << s.loopGap << ',' << s.events << ',' << s.update << ','
             << s.queries << ',' << s.glCheck << ',' << s.swap << ',' << s.other << ',' << s.post << ','
             << s.title << ',' << s.titleUpdated << ',' << s.storageGrew << ',' << s.frame + s.post
             << ',' << s.work + s.post << ',' << s.regionTests << ',' << s.regionsRejected << ','
             << s.objectCandidates << ',' << s.objectsRegionSkipped << ',' << lateness << ','
             << s.trace.total << ',' << s.trace.state << ',' << s.trace.shadowPrepare << ','
             << s.trace.shadowSubmit << ',' << s.trace.sceneSubmit << ',' << s.trace.composite << ','
             << s.trace.shadowUpload << ',' << s.trace.shadowDraw << ',' << s.trace.sceneUpload << ','
             << s.trace.sceneDraw << ',' << s.queryBegin << ',' << s.queryEnd << ',' << s.hud << ','
             << s.fogVisible << ',' << s.fogExplored << ',' << s.fogRejected << ',' << s.fogUpdate << ','
             << s.cameraEye.x << ',' << s.cameraEye.y << ',' << s.cameraEye.z << ',' << s.cameraTarget.x
             << ',' << s.cameraTarget.y << ',' << s.cameraTarget.z << ',' << s.terrainCoarse << ','
             << s.canopyNear << ',' << s.canopyMid << ',' << s.canopyFar << ',' << s.canopyCacheHits
             << ',' << s.canopyCacheMisses << ',' << s.canopyCacheBytes << '\n';
        csv << line.str();
        if (s.work + s.post > 1000.0 / 60 || s.loopGap > 1000.0 / 60 || lateness > 1000.0 / 60) {
            spikes << line.str();
            ++spikeCount;
        }
        maxCycle = std::max(maxCycle, s.frame + s.post);
        maxActive = std::max(maxActive, s.work + s.post);
        maxGap = std::max(maxGap, s.loopGap);
        times.push_back(s.frame);
        cpuTimes.push_back(s.cpu);
        if (s.gpu >= 0)
            gpuTimes.push_back(s.gpu);
    }
    auto mean = [](const std::vector<double> &v) {
        return v.empty() ? 0 : std::accumulate(v.begin(), v.end(), 0.) / v.size();
    };
    std::vector<double> updateTimes;
    int peakChunks = 0, totalGenerated = 0, totalUnloaded = 0;
    for (const auto &sample : all) {
        peakChunks = std::max(peakChunks, sample.world.chunks);
        totalGenerated += sample.world.generated;
        totalUnloaded += sample.world.unloaded;
        if (sample.measured)
            updateTimes.push_back(sample.world.generationMs);
    }
    Sample last = all.empty() ? Sample{} : all.back();
    std::ofstream json(std::filesystem::path(o.output) / "summary.json");
    if (!json)
        throw std::runtime_error("Cannot write summary.json");
    json << std::fixed << std::setprecision(5)
         << "{\n  \"engine\": \"Pleiade\",\n  \"version\": " << quote(version) << ",\n  \"scene\": "
         << quote(o.shore       ? "Shore-001"
                  : o.reference ? "Grove-001"
                  : o.world     ? "Field-001"
                                : "Courtyard-001")
         << ",\n  \"vendor\": " << quote(r.vendor) << ",\n  \"renderer\": " << quote(r.gpu)
         << ",\n  \"opengl\": " << quote(r.api)
         << ",\n  \"sdl_video_driver\": " << quote(SDL_GetCurrentVideoDriver())
         << ",\n  \"width\": " << r.width << ",\n  \"height\": " << r.height
         << ",\n  \"camera\": "
         << quote(o.cinematic ? "cinematic" : o.strategy ? "strategy-perspective"
                                                         : o.walkDemo ? "first-person-player" : "fps-inspection")
         << ",\n  \"benchmark\": " << (o.benchmark ? "true" : "false")
         << ",\n  \"diagnostics_schema\": 7"
         << ",\n  \"terrain_lod_enabled\": " << (o.terrainLOD ? "true" : "false")
         << ",\n  \"fow_enabled\": " << (o.fowDemo ? "true" : "false")
         << ",\n  \"fow_culling\": " << (o.fowCulling ? "true" : "false")
         << ",\n  \"fow_visible_cells\": " << (fog ? fog->visibleCount : 0)
         << ",\n  \"fow_explored_cells\": " << (fog ? fog->exploredCount : 0)
         << ",\n  \"fow_updates\": " << (fog ? fog->revision : 0)
         << ",\n  \"vision_radius_m\": " << o.visionRadius
         << ",\n  \"gpu_timing_enabled\": " << (o.gpuTiming ? "true" : "false")
         << ",\n  \"trace_render_enabled\": " << (o.traceRender ? "true" : "false")
         << ",\n  \"title_updates_enabled\": " << (o.titleUpdates ? "true" : "false")
         << ",\n  \"spike_threshold_ms\": " << 1000.0 / 60
         << ",\n  \"diagnostic_spike_count\": " << spikeCount << ",\n  \"max_cycle_ms\": " << maxCycle
         << ",\n  \"max_deadline_lateness_ms\": " << maxLateness
         << ",\n  \"max_active_cycle_ms\": " << maxActive << ",\n  \"max_loop_gap_ms\": " << maxGap
         << ",\n  \"vsync_actual\": " << SDL_GL_GetSwapInterval()
         << ",\n  \"stress_objects\": " << o.stress
         << ",\n  \"frame_gl_checks\": " << (o.frameGlChecks ? "true" : "false")
         << ",\n  \"region_culling\": " << (o.regionCulling ? "true" : "false")
         << ",\n  \"culling\": " << (o.culling ? "true" : "false")
         << ",\n  \"shadows\": " << (o.shadows ? "true" : "false")
         << ",\n  \"shadow_size\": " << r.shadowSize << ",\n  \"shadow_policy\": "
         << quote(o.world ? "world-anchored texels; strategy volume sized by zoom; grass does not cast shadows"
                          : "static scene cached after first draw")
         << ",\n  \"spatial_antialiasing\": " << (o.reference && o.aa ? "true" : "false")
         << ",\n  \"world_seed\": " << o.seed << ",\n  \"lod_enabled\": " << (o.lod ? "true" : "false")
         << ",\n  \"world_radius\": " << o.worldRadius
         << ",\n  \"actors\": " << (o.world ? o.actors + (o.fowDemo ? ScoutGroup::size : 0) : 0)
         << ",\n  \"chunk_budget_per_frame\": " << o.chunkBudget
         << ",\n  \"generation_soft_budget_ms\": 2.0"
         << ",\n  \"peak_resident_chunks\": " << peakChunks
         << ",\n  \"final_resident_chunks\": " << last.world.chunks
         << ",\n  \"final_pending_chunks\": " << last.world.pending
         << ",\n  \"chunks_generated_total\": " << totalGenerated
         << ",\n  \"chunks_unloaded_total\": " << totalUnloaded
         << ",\n  \"mean_world_update_ms\": " << mean(updateTimes)
         << ",\n  \"p95_world_update_ms\": " << percentile(updateTimes, .95)
         << ",\n  \"final_trees\": " << last.world.trees
         << ",\n  \"final_grass_tufts\": " << last.world.grass
         << ",\n  \"final_structures\": " << last.world.structures
         << ",\n  \"simulation_hz\": 60,\n  \"dropped_simulation_steps\": " << last.droppedSteps
         << ",\n  \"pbr_materials\": " << (o.materials ? "true" : "false")
         << ",\n  \"water_enabled\": " << (o.shore && o.water ? "true" : "false")
         << ",\n  \"water_ssr\": " << (o.shore && o.water && o.waterSSR ? "true" : "false")
         << ",\n  \"material_texture_mib\": 32.0"
         << ",\n  \"canopy_layers\": " << (o.layers ? "true" : "false")
         << ",\n  \"natural_quality\": " << (o.natural ? "true" : "false") << ",\n  \"ao\": "
         << quote(o.natural ? "screen-space ambient contact, 12 samples; not CORTAO"
                            : "none - CORTAO not integrated")
         << ",\n  \"estimated_render_target_mib\": " << r.targetMiB()
         << ",\n  \"requested_measured_frames\": " << o.frames
         << ",\n  \"completed_requested_run\": " << (complete ? "true" : "false")
         << ",\n  \"measurement_mode\": " << quote(o.seconds ? "wall-clock" : "frame-count")
         << ",\n  \"requested_measurement_seconds\": " << o.seconds
         << ",\n  \"requested_warmup_seconds\": " << (o.seconds ? o.warmupSeconds : 0)
         << ",\n  \"actual_warmup_seconds\": " << actualWarmupSeconds
         << ",\n  \"measured_wall_seconds\": " << wallSeconds
         << ",\n  \"wall_fps\": " << (wallSeconds > 0 ? times.size() / wallSeconds : 0)
         << ",\n  \"pacing_backend\": " << quote(pacer.backend())
         << ",\n  \"pacing_resyncs\": " << pacer.resyncs
         << ",\n  \"pacing_wait_failures\": " << pacer.waitFailures << ",\n  \"max_fps\": " << o.maxFPS
         << ",\n  \"mean_work_frame_ms\": " << mean(workTimes)
         << ",\n  \"mean_pacing_ms\": " << mean(pacingTimes)
         << ",\n  \"warmup_frames\": " << warmupFrames << ",\n  \"measured_frames\": " << times.size()
         << ",\n  \"gpu_samples\": " << gpuTimes.size() << ",\n  \"mean_frame_ms\": " << mean(times)
         << ",\n  \"p50_frame_ms\": " << percentile(times, .50)
         << ",\n  \"p95_frame_ms\": " << percentile(times, .95)
         << ",\n  \"p99_frame_ms\": " << percentile(times, .99)
         << ",\n  \"mean_fps\": " << (mean(times) > 0 ? 1000 / mean(times) : 0)
         << ",\n  \"mean_cpu_submit_ms\": " << mean(cpuTimes) << ",\n  \"mean_gpu_render_ms\": ";
    if (gpuTimes.empty())
        json << "null";
    else
        json << mean(gpuTimes);
    json << ",\n  \"p95_gpu_render_ms\": ";
    if (gpuTimes.empty())
        json << "null";
    else
        json << percentile(gpuTimes, .95);
    json << ",\n  \"resize_events\": " << resizeCount << ",\n  \"camera_toggle_events\": " << toggleCount
         << ",\n  \"gpu_timing_scope\": \"scene + composite + optional HUD; excludes swap/readback\",\n "
            " \"frame_timing_scope\": \"event processing + update + submit + swap; excludes screenshots "
            "and report writing; includes deliberate FPS pacing\",\n  \"memory_note\": \"Attachment "
            "allocation estimate only. Not "
            "measured total VRAM or RAM.\"\n}\n";
    csv.close();
    spikes.close();
    json.close();
    if (!csv || !spikes || !json)
        throw std::runtime_error("Failed to finish report files");
    std::cout << "Measured " << times.size() << " frames. Mean " << fixed(mean(times)) << " ms; P95 "
              << fixed(percentile(times, .95)) << " ms. GPU samples " << gpuTimes.size()
              << ".\nReports: " << o.output << "\n";
}
CinematicTrack demoTrack(const Options &o) {
    if (o.shore)
        return CinematicTrack({
            {0, {8, 3.2f, 9}, {-2, 1.3f, -4}, 55},
            {3, {4.4f, 2.25f, 6.8f}, {-2.1f, 1.2f, -4.8f}, 52},
            {6, {2.8f, 1.35f, 3.8f}, {-2.9f, .8f, -5}, 49},
            {6.8, {-8, 3.9f, 2.5f}, {5.7f, 2.8f, -6.8f}, 52, true},
            {10, {-6.5f, 5.2f, -.8f}, {5.6f, 3.1f, -7.4f}, 58},
        });
    V3 offset = o.world ? V3{18, terrainHeight(18, 16), 16} : V3{};
    return CinematicTrack({
        {0, offset + V3{10, 5, 21}, offset + V3{0, 2.7f, -2}, 55},
        {3, offset + V3{5, 3.8f, 16}, offset + V3{0, 2.5f, -2}, 52},
        {6, offset + V3{2, 2.8f, 10}, offset + V3{0, 2.75f, -1}, 47},
        {6.8, offset + V3{-9, 6, 9}, offset + V3{1, 3.2f, -2}, 54, true},
        {10, offset + V3{-6, 7.4f, 7}, offset + V3{1, 3.2f, -2.5f}, 60},
    });
}
int run(int argc, char **argv) {
    Options o = parse(argc, argv);
    Platform platform(o);
    int w, h;
    SDL_GL_GetDrawableSize(platform.window, &w, &h);
    Renderer renderer(w, h, o.shadowSize, o.gpuTiming, o.assetDirectory);
    Scene scene(o.stress, !o.world && !o.shore);
    if (o.shore)
        scene = makeShoreScene();
    std::unique_ptr<ProceduralWorld> world;
    if (o.world)
        world = std::make_unique<ProceduralWorld>(uint32_t(o.seed), o.worldRadius, o.reference);
    std::unique_ptr<VisibilityMap> visibility;
    ScoutGroup scoutGroup;
    double lastVisibilityTime = -1;
    if (o.fowDemo)
        visibility = std::make_unique<VisibilityMap>();
    FixedSimulation simulation;
    PlayerController player;
    WorldStats worldStats;
    size_t worldBaseCount = 0;
    int simSteps = 0;
    bool treeInspectionPlaced = false;
    Camera camera = o.world ? worldTour(0, o.strategy) : tour(0, o.strategy);
    if (o.shore) {
        camera.pos = {8, 3.2f, 9};
        camera.target = {0, 0, 0};
        camera.distance = 18;
        camera.face({-2, 0, -2});
    }
    camera.orbit = o.inspectionAngle * pi / 180;
    camera.yaw += o.inspectionAngle * pi / 180;
    if (o.fowDemo)
        camera.distance = 75;
    if (o.strategyDistance)
        camera.distance = float(o.strategyDistance);
    const auto walkGround = [&](float x, float z) {
        constexpr float radius = .27f;
        const float invalid = std::numeric_limits<float>::quiet_NaN();
        if (o.world)
            return terrainHeight(x, z);
        if (o.shore)
            return std::abs(x) <= 24 - radius && std::abs(z) <= 24 - radius &&
                           shoreHeight(x, z) >= .08f
                       ? shoreHeight(x, z)
                       : invalid;
        if (std::abs(x) > 90 - radius || std::abs(z) > 90 - radius)
            return invalid;
        constexpr float yardLevel = -.15f;
        float surface = yardLevel;
        for (const auto &object : scene.objects) {
            if (object.mesh != MeshType::Cube || object.center.y - object.half.y > yardLevel + 1.9f ||
                std::abs(x - object.center.x) > object.half.x + radius ||
                std::abs(z - object.center.z) > object.half.z + radius)
                continue;
            surface = std::max(surface, object.center.y + object.half.y);
        }
        return surface;
    };
    if (o.walkDemo) {
        if (!o.world && !o.shore) {
            camera.pos = {0, 0, 19.5f};
            camera.face({0, 1.f, -1});
            camera.pitch = -.05f;
        }
        if (!player.spawn(camera.pos, walkGround))
            throw std::runtime_error("The player spawn is outside the walkable surface");
        camera.pos = player.eye();
    }
    std::optional<CinematicTrack> cinematic;
    if (o.cinematic)
        cinematic = demoTrack(o);
    struct StrategyClick {
        float x, y;
        bool additive;
    };
    struct StrategyView {
        Camera camera;
        float aspect;
    };
    std::vector<StrategyClick> pendingPicks;
    std::optional<StrategyView> presentedStrategy;
    Uint16 modifiers = SDL_GetModState();
    const char *orderStatus = nullptr;
    bool jumpQueued = false;
    std::cout << "PLEIADE " << version << "\n"
              << renderer.vendor << " | " << renderer.gpu << " | " << renderer.api << "\n";
    if (renderer.gpu.find("llvmpipe") != std::string::npos)
        std::cout << "SOFTWARE RENDERER: results do not predict RTX 2060 performance.\n";
    std::cout << "TAB camera, RMB orbit/look, WASD move, wheel strategy zoom, Shift fast\n"
              << (o.walkDemo ? "Space jump\n" : "Q/E inspection height\n")
              << "F1 help, F2 culling, F3 shadows, F4 HUD, F12 screenshot, Escape exit\n";
    if (visibility)
        std::cout << "LMB select or order, Shift+LMB add scout\n";
    const int limit = o.frames ? o.frames + o.warmup : 0;
    std::vector<Sample> samples;
    samples.reserve(limit ? limit : 10000);
    FramePacer pacer(o.maxFPS, o.legacyPacing);
    UI ui;
    bool running = true, help = false, mouse = false;
    int frame = 0, resizes = 0, toggles = 0;
    float exposure = o.reference ? .95f : 1.05f;
    double lastGPU = -1, smoothedFrame = 16.67;
    auto previous = Clock::now();
    const auto runStart = previous;
    auto previousLoopEnd = previous;
    BenchmarkClock timer(o.warmupSeconds, o.seconds ? o.seconds : 1);
    double measuredWall = 0, actualWarmup = 0;
    bool finished = false;
    Clock::time_point firstMeasured{};
    while (running && (limit == 0 || frame < limit)) {
        auto start = Clock::now();
        double loopGap = milliseconds(previousLoopEnd, start);
        double elapsed = milliseconds(runStart, start) / 1000.0;
        if (o.seconds && timer.complete(elapsed)) {
            finished = true;
            break;
        }
        bool measuring = o.seconds ? timer.beginFrame(elapsed) : frame >= o.warmup;
        if (measuring && firstMeasured == Clock::time_point{}) {
            firstMeasured = start;
            actualWarmup = elapsed;
        }
        double realDt = milliseconds(previous, start) / 1000.0;
        float dt = std::min(float(realDt), .05f);
        previous = start;
        auto eventsStart = Clock::now();
        if (o.resizeTest) {
            if (frame == 3 || frame == 7) {
                SDL_Event e{};
                e.type = SDL_KEYDOWN;
                e.key.keysym.sym = SDLK_TAB;
                SDL_PushEvent(&e);
            }
            if (frame == 5)
                SDL_SetWindowSize(platform.window, 960, 540);
            if (frame == 10)
                SDL_SetWindowSize(platform.window, o.width, o.height);
        }
        SDL_Event event{};
        bool screenshot = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                modifiers = event.key.keysym.mod;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mouse = false;
                jumpQueued = false;
                modifiers = KMOD_NONE;
                pendingPicks.clear();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                modifiers = SDL_GetModState();
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT &&
                !o.benchmark && !cinematic) {
                mouse = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
                mouse = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if (event.type == SDL_MOUSEMOTION && mouse && !o.benchmark && !cinematic) {
                if (camera.strategy)
                    camera.orbit -= event.motion.xrel * .0025f;
                else {
                    camera.yaw -= event.motion.xrel * .0025f;
                    camera.pitch = std::clamp(camera.pitch - event.motion.yrel * .0025f, -1.5f, 1.5f);
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
                presentedStrategy && camera.strategy && !mouse && !o.benchmark) {
                int windowWidth, windowHeight;
                SDL_GetWindowSize(platform.window, &windowWidth, &windowHeight);
                if (windowWidth > 0 && windowHeight > 0)
                    pendingPicks.push_back({2.f * (event.button.x + .5f) / windowWidth - 1,
                                            1 - 2.f * (event.button.y + .5f) / windowHeight,
                                            (modifiers & KMOD_SHIFT) != 0});
            }
            if (event.type == SDL_MOUSEWHEEL && camera.strategy && !o.benchmark)
                camera.distance = std::clamp(camera.distance - event.wheel.y * 2.f, 8.f, 100.f);
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                auto key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE)
                    running = false;
                if (key == SDLK_F1 && !o.benchmark)
                    help = !help;
                if (!o.benchmark) {
                    if (key == SDLK_SPACE && o.walkDemo && !camera.strategy)
                        jumpQueued = true;
                    if (key == SDLK_TAB && !cinematic) {
                        if (!camera.strategy && o.walkDemo) {
                            camera.target = player.position();
                            camera.distance = 22;
                        }
                        camera.strategy = !camera.strategy;
                        scoutGroup.selected.fill(false);
                        orderStatus = nullptr;
                        jumpQueued = false;
                        pendingPicks.clear();
                        ++toggles;
                    }
                    if (key == SDLK_F2)
                        o.culling = !o.culling;
                    if (key == SDLK_F3)
                        o.shadows = !o.shadows;
                    if (key == SDLK_F4)
                        o.hud = !o.hud;
                    if (key == SDLK_F9)
                        o.materials = !o.materials;
                    if (key == SDLK_F11 && o.shore)
                        o.waterSSR = !o.waterSSR;
                    if (key == SDLK_F10 && o.shore)
                        o.water = !o.water;
                    if (key == SDLK_F8)
                        o.layers = !o.layers;
                    if (key == SDLK_F7)
                        o.natural = !o.natural;
                    if (key == SDLK_F6)
                        o.terrainLOD = !o.terrainLOD;
                    if (key == SDLK_F5)
                        o.lod = !o.lod;
                    if (key == SDLK_F12)
                        screenshot = true;
                }
            }
        }
        if (!running)
            break;
        if ((SDL_GetWindowFlags(platform.window) & SDL_WINDOW_MINIMIZED) != 0) {
            if (o.benchmark) {
                running = false;
                break;
            }
            pacer.reset();
            SDL_Delay(30);
            previous = Clock::now();
            continue;
        }
        SDL_GL_GetDrawableSize(platform.window, &w, &h);
        if (w <= 0 || h <= 0) {
            if (o.benchmark) {
                running = false;
                break;
            }
            pacer.reset();
            SDL_Delay(10);
            continue;
        }
        if (w != renderer.width || h != renderer.height) {
            if (o.benchmark) {
                running = false;
                break;
            }
            renderer.resize(w, h);
            ++resizes;
        }
        auto eventsEnd = Clock::now();
        double fogUpdateMs = 0;
        const bool fixedStepPlayback = (o.benchmark && !o.seconds) || (cinematic && limit);
        if (o.world || o.shore || o.walkDemo)
            simSteps = simulation.advance(fixedStepPlayback ? FixedSimulation::step : realDt);
        if (cinematic) {
            const auto pose = cinematic->sample(o.cinematicTime + (limit ? frame * FixedSimulation::step : elapsed));
            camera.strategy = false;
            camera.pos = pose.position;
            camera.target = pose.target;
            camera.face(pose.target);
            camera.verticalFovDegrees = pose.verticalFovDegrees;
        } else if (o.benchmark && !o.world && !o.fixedCamera)
            camera = o.seconds
                         ? tour(static_cast<int>(
                                    (5.0 - 5.0 * std::cos(2.0 * pi *
                                                          (measuring ? timer.measuredSeconds(elapsed)
                                                                     : elapsed) /
                                                          10.0)) *
                                    60),
                                o.strategy)
                         : tour(std::max(0, frame - o.warmup), o.strategy);
        else {
            auto keys = SDL_GetKeyboardState(nullptr);
            float right = float(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]);
            float forward = float(keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]);
            if (o.walkDemo && !camera.strategy) {
                for (int i = 0; i < simSteps; ++i) {
                    player.step({right, forward, keys[SDL_SCANCODE_LSHIFT] != 0, jumpQueued},
                                camera.yaw, walkGround);
                    jumpQueued = false;
                }
                camera.pos = player.eye(float((simulation.renderTime() - simulation.time()) / FixedSimulation::step));
            } else
                manualCameraMotion(camera, o.benchmark, right, forward,
                                   float(keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q]), dt,
                                   keys[SDL_SCANCODE_LSHIFT]);
        }
        if (o.world) {
            double pathTime = o.benchmark
                                  ? (o.seconds ? (measuring ? timer.measuredSeconds(elapsed) : 0.0)
                                               : std::max(0, frame - o.warmup) / 60.0)
                                  : 0.0;
            if (o.benchmark && !o.fowDemo && !o.fixedCamera)
                camera = worldTour(pathTime, o.strategy);
            if (o.benchmark && o.strategyDistance)
                camera.distance = float(o.strategyDistance);
            if (!camera.strategy && !cinematic && !o.walkDemo)
                camera.pos.y = std::max(camera.pos.y, terrainHeight(camera.pos.x, camera.pos.z) + 1.7f);
            scene.objects.resize(worldBaseCount);
            worldStats =
                world->update(o.fowDemo ? V3{8, 0, 6} : (camera.strategy ? camera.target : camera.pos),
                              scene, o.chunkBudget, 2.0);
            worldBaseCount = scene.objects.size();
            if (o.inspectTree && !treeInspectionPlaced && worldStats.pending == 0) {
                const Object *chosen = nullptr;
                float best = 1e9f;
                for (const auto &object : scene.objects)
                    if (object.mesh == MeshType::Leaves ||
                        (object.vegetation == VegetationSection::Collar && object.lodClass == 0)) {
                        float d = length(object.center - V3{8, 0, 6});
                        if (d < best) {
                            best = d;
                            chosen = &object;
                        }
                    }
                if (chosen) {
                    camera.target = chosen->center;
                    camera.distance = 8;
                    float distance = std::max({chosen->half.x, chosen->half.y, chosen->half.z}) * 3.5f;
                    camera.pos = chosen->center + V3{distance * .65f, 0, distance * .76f};
                    V3 aim = unit(chosen->center - camera.pos);
                    camera.yaw = std::atan2(aim.x, aim.z);
                    camera.pitch = std::asin(aim.y);
                    treeInspectionPlaced = true;
                }
            }
            scene.time = o.freeze ? 0 : simulation.renderTime() + (cinematic ? o.cinematicTime : 0);
            appendActors(scene, scene.time, o.actors);
            if (visibility) {
                auto fogStart = Clock::now();
                visibility->rebuild(scene);
                if (presentedStrategy && !pendingPicks.empty() && worldStats.pending == 0) {
                    constexpr uint64_t structureId = 0;
                    std::vector<StrategyPickTarget> targets;
                    for (size_t i = 0; i < ScoutGroup::size; ++i)
                        targets.push_back({i + 1, scoutGroup.scouts[i].position + V3{0, .9f, 0},
                                           {.75f, .9f, .75f}});
                    for (const auto &object : scene.objects)
                        if (object.visible && (object.mesh == MeshType::Cube || object.mesh == MeshType::Roof ||
                                               object.mesh == MeshType::HipRoof))
                            targets.push_back({structureId, object.center, object.half});
                    const auto loadedHeight = [&](float x, float z) {
                        return visibility->residentCell(visibility->index(x, z))
                                   ? terrainHeight(x, z) : std::numeric_limits<float>::quiet_NaN();
                    };
                    for (const auto &click : pendingPicks) {
                        const auto hit = pickStrategy(presentedStrategy->camera, click.x, click.y,
                                                      presentedStrategy->aspect, targets, loadedHeight);
                        if (hit && hit->entity && *hit->entity != structureId) {
                            scoutGroup.select(size_t(*hit->entity - 1), click.additive);
                            orderStatus = nullptr;
                        } else if (hit && !hit->entity && !click.additive && scoutGroup.selectionCount()) {
                            orderStatus = scoutGroup.command(*visibility, hit->point) ? "ORDER ACCEPTED" : "ROUTE UNAVAILABLE";
                        } else if (!hit && !click.additive) {
                            scoutGroup.selected.fill(false);
                            orderStatus = nullptr;
                        }
                    }
                }
                pendingPicks.clear();
                if (worldStats.pending == 0)
                    for (auto &scout : scoutGroup.scouts)
                        scout.advance(*visibility,
                                      o.freeze ? 0.0 : (o.benchmark && !o.seconds ? 1.0 / 60.0 : realDt));
                if (worldStats.pending == 0 &&
                    (lastVisibilityTime < 0 || scene.time - lastVisibilityTime >= .1 ||
                     visibility->revision == 0)) {
                    std::array<V3, ScoutGroup::size> observers{};
                    for (size_t i = 0; i < ScoutGroup::size; ++i)
                        observers[i] = scoutGroup.scouts[i].position;
                    visibility->reveal(observers, float(o.visionRadius));
                    lastVisibilityTime = scene.time;
                }
                if (worldStats.pending == 0)
                    for (const auto &scout : scoutGroup.scouts)
                        scout.append(scene, scene.time);
                fogUpdateMs = milliseconds(fogStart, Clock::now());
                renderer.setVisibility(visibility.get(), o.fowCulling);
            }
        }
        if (o.shore)
            scene.time = (o.freeze ? 0.0 : simulation.renderTime() + (cinematic ? o.cinematicTime : 0)) +
                         o.waterTime;
        auto updateEnd = Clock::now();
        for (auto [id, ms] : renderer.collectGPU()) {
            if (id < int(samples.size()))
                samples[id].gpu = ms;
            lastGPU = ms;
        }
        auto cpuStart = Clock::now();
        renderer.beginGPU(frame);
        auto queryBeginEnd = o.traceRender ? Clock::now() : Clock::time_point{};
        auto stats = renderer.render(scene, camera, o.culling, o.shadows, exposure, o.lod,
                                     o.reference && o.aa, o.regionCulling, o.traceRender, o.terrainLOD,
                                     o.natural, o.layers, o.materials, o.shore && o.water, o.waterSSR, o.waveStrength/100.f);
        auto renderEnd = o.traceRender ? Clock::now() : Clock::time_point{};
        if (o.hud) {
            ui.vertices.clear();
            V4 white{.90f, .94f, .92f, 1}, muted{.56f, .66f, .68f, 1}, mint{.54f, .88f, .71f, 1};
            float scale = w >= 1100 ? 2.f : 1.f;
            ui.rect(20, 20, 290 * scale, 76 * scale, {.025f, .045f, .055f, .90f});
            ui.rect(20, 20, 3, 76 * scale, mint);
            ui.text(20 + 10 * scale, 20 + 9 * scale, "PLEIADE", 3 * scale, white);
            ui.text(20 + 10 * scale, 20 + 36 * scale, std::string("ENGINE LAB / ") + version, scale,
                    muted);
            ui.text(20 + 10 * scale, 20 + 53 * scale,
                    cinematic ? "CINEMATIC" : camera.strategy ? "02  /  STRATEGY VIEW"
                                : o.walkDemo ? "01  /  FIRST PERSON WALK" : "01  /  FIRST PERSON INSPECTION",
                    scale, mint);
            float y = float(h) - 84 * scale - 20;
            ui.rect(20, y, 290 * scale, 84 * scale, {.025f, .045f, .055f, .91f});
            ui.text(20 + 10 * scale, y + 10 * scale,
                    "FRAME " + fixed(smoothedFrame) + " MS   GPU " +
                        (lastGPU >= 0 ? fixed(lastGPU) : "--") + " MS",
                    scale, white);
            ui.text(20 + 10 * scale, y + 26 * scale,
                    "VISIBLE " + std::to_string(stats.visible) + " / " + std::to_string(stats.total) +
                        "   DRAWS " + std::to_string(stats.sceneDraws),
                    scale, muted);
            ui.text(20 + 10 * scale, y + 42 * scale,
                    std::string("CULL ") + (o.culling ? "ON" : "OFF") + "   SHADOW " +
                        (o.shadows ? "ON" : "OFF") + "   AO NONE",
                    scale, muted);
            ui.text(20 + 10 * scale, y + 62 * scale,
                    o.benchmark ? "BENCHMARK / FIXED CAMERA PATH"
                                : cinematic ? "REAL TIME CAMERA TRACK"
                                            : ("TAB CAMERA / CAP " + std::to_string(o.maxFPS) + " FPS"),
                    scale, mint);
            if (help) {
                float yy = 110 * scale;
                ui.rect(20, yy, 290 * scale, (visibility ? 105 : 90) * scale,
                        {.025f, .045f, .055f, .94f});
                int row = 0;
                for (auto s : {"WASD MOVE / SHIFT FAST",
                               o.walkDemo ? "RIGHT MOUSE LOOK / SPACE JUMP" : "RIGHT MOUSE LOOK / Q E HEIGHT",
                               "WHEEL ZOOM IN STRATEGY MODE", "F2 CULL / F3 SHADOW / F4 HUD",
                               "F9 MATERIALS / F10 WATER / F12 CAPTURE"})
                    ui.text(20 + 10 * scale, yy + (10 + row++ * 15) * scale, s, scale, white);
                if (visibility)
                    ui.text(20 + 10 * scale, yy + (10 + row * 15) * scale,
                            "CLICK SCOUT / SHIFT CLICK ADD", scale, white);
            }
            if (o.world) {
                float x = float(w) - 20 - 260 * scale;
                ui.rect(x, 20, 260 * scale, 128 * scale, {.025f, .045f, .055f, .92f});
                ui.text(
                    x + 10 * scale, 30,
                    std::string(o.reference ? "GROVE REFERENCE / SEED " : "PROCEDURAL FIELD / SEED ") +
                        std::to_string(o.seed),
                    scale, mint);
                int row = 0;
                for (const auto &line : std::vector<std::string>{
                         "CHUNKS " + std::to_string(worldStats.chunks) + " / QUEUED " +
                             std::to_string(worldStats.pending),
                         "TREES " + std::to_string(worldStats.trees) + " / GRASS " +
                             std::to_string(worldStats.grass),
                         "STRUCTURES " + std::to_string(worldStats.structures) + " / ACTORS " +
                             std::to_string(o.actors + (o.fowDemo ? ScoutGroup::size : 0)),
                         "UPDATE " + fixed(worldStats.generationMs) + " MS / FAR LOD " +
                             std::to_string(stats.lodFar),
                         "TRIANGLES " + std::to_string(stats.triangles),
                         "F7 NATURAL " + std::string(o.natural ? "ON" : "OFF") + "  F6 FAR " +
                             std::to_string(stats.terrainCoarse),
                         "F8 LAYERS " + std::string(o.layers ? "ON " : "OFF ") +
                             std::to_string(stats.canopyNear) + "/" + std::to_string(stats.canopyMid) +
                             "/" + std::to_string(stats.canopyFar),
                         std::string("F5 LOD ") + (o.lod ? "ON" : "OFF") + " / SIM 60 HZ"}) {
                    ui.text(x + 10 * scale, 20 + (25 + row++ * 12) * scale, line, scale, white);
                }
            }
            if (visibility) {
                float mx = float(w) - 184, my = float(h) - 170;
                ui.rect(mx - 4, my - 26, 168, 190, {.015f, .02f, .025f, 1});
                const auto selectedCount = scoutGroup.selectionCount();
                ui.text(mx, my - 22, "SCOUTS " + std::to_string(selectedCount) + "/" +
                            std::to_string(ScoutGroup::size) + " SELECTED", 1, {.7f, .9f, 1, 1});
                const char *hint = worldStats.pending > 0 ? "LOADING TERRAIN"
                                   : !camera.strategy    ? "TAB STRATEGY VIEW"
                                   : orderStatus         ? orderStatus
                                   : selectedCount       ? "CLICK TERRAIN TO MOVE"
                                                         : "CLICK SCOUT TO SELECT";
                ui.text(mx, my - 11, hint, 1, {.7f, .9f, 1, 1});
                for (int z = 0; z < 32; ++z)
                    for (int x = 0; x < 32; ++x) {
                        int visibleCell = 0, exploredCell = 0;
                        for (int dz = 0; dz < 4; ++dz)
                            for (int dx = 0; dx < 4; ++dx) {
                                int i = ((z * 4 + dz) * 128 + x * 4 + dx) * 2;
                                visibleCell += visibility->pixels[i] != 0;
                                exploredCell += visibility->pixels[i + 1] != 0;
                            }
                        V4 color = visibleCell    ? V4{.25f, .65f, .4f, 1}
                                   : exploredCell ? V4{.09f, .19f, .15f, 1}
                                                  : V4{.015f, .02f, .025f, 1};
                        ui.rect(mx + x * 5, my + z * 5, 5, 5, color);
                    }
                const M4 viewProjection = camera.projection(float(w) / h) * camera.view();
                constexpr float mapScale = 160 / (VisibilityMap::size * VisibilityMap::cell);
                for (size_t i = 0; i < ScoutGroup::size && worldStats.pending == 0; ++i) {
                    const V3 p = scoutGroup.scouts[i].position;
                    const V4 marker = scoutGroup.selected[i] ? mint : V4{.2f, .8f, 1, 1};
                    float sx = (p.x - VisibilityMap::origin) * mapScale,
                          sz = (p.z - VisibilityMap::origin) * mapScale;
                    ui.rect(mx + sx - 1.5f, my + sz - 1.5f, 3, 3, marker);
                    const auto scoutClip = viewProjection * V4{p.x, p.y + 2.2f, p.z, 1};
                    if (scoutClip.w <= 0)
                        continue;
                    float px = (scoutClip.x / scoutClip.w * .5f + .5f) * w,
                          py = (.5f - scoutClip.y / scoutClip.w * .5f) * h;
                    if (px > 20 && px < w - 60 && py > 20 && py < h - 20) {
                        ui.rect(px - 3, py - 3, 6, 6, marker);
                        ui.text(px - 3, py - 14, std::to_string(i + 1), 1, marker);
                    }
                }
            }
            renderer.drawUI(ui);
        }
        auto hudEnd = o.traceRender ? Clock::now() : Clock::time_point{};
        renderer.endGPU();
        auto cpuEnd = Clock::now();
        if (o.frameGlChecks)
            renderer.gl.check("Frame");
        auto checkEnd = Clock::now();
        // Readback before swap so captures contain the actual rendered back buffer.
        auto captureStart = Clock::now();
        bool captureNow = screenshot || (!o.capture.empty() && limit && frame == limit - 1);
        if (captureNow) {
            std::filesystem::create_directories(o.output);
            renderer.capture(screenshot ? (std::filesystem::path(o.output) / "capture.bmp").string()
                                        : o.capture);
        }
        double captureMs = captureNow ? milliseconds(captureStart, Clock::now()) : 0;
        auto swapStart = Clock::now();
        SDL_GL_SwapWindow(platform.window);
        if (visibility && camera.strategy && worldStats.pending == 0)
            presentedStrategy = StrategyView{camera, float(w) / h};
        else
            presentedStrategy.reset();
        auto workEnd = Clock::now();
        pacer.wait(start);
        auto end = Clock::now();
        double frameMs = milliseconds(start, end) - captureMs;
        bool storageGrew = samples.size() == samples.capacity();
        samples.push_back({frameMs, milliseconds(cpuStart, cpuEnd), -1, stats.cullMs, stats.visible,
                           stats.sceneDraws, stats.triangles, milliseconds(start, workEnd) - captureMs,
                           milliseconds(workEnd, end), measuring, worldStats, stats.lodFar,
                           stats.distanceSkipped, stats.shadowDraws, simSteps, simulation.dropped});
        smoothedFrame = smoothedFrame * .93 + frameMs * .07;
        double titleMs = 0;
        bool titleUpdated = o.titleUpdates && frame % 30 == 0;
        if (titleUpdated) {
            auto titleStart = Clock::now();
            std::string title = std::string("PLEIADE ") + version + " | " +
                                (cinematic ? "CINEMATIC" : camera.strategy ? "STRATEGY" : "FPS") +
                                " | " + fixed(smoothedFrame) +
                                " ms | GPU " + (lastGPU >= 0 ? fixed(lastGPU) + " ms" : "--");
            if (o.seconds)
                title += measuring ? " | MEASURE " + fixed(timer.measuredSeconds(elapsed), 1) + "/" +
                                         std::to_string(o.seconds) + " s"
                                   : " | WARMUP " + fixed(elapsed, 1) + "/" +
                                         std::to_string(o.warmupSeconds) + " s";
            SDL_SetWindowTitle(platform.window, title.c_str());
            titleMs = milliseconds(titleStart, Clock::now());
        }
        auto postEnd = Clock::now();
        auto &sample = samples.back();
        sample.terrainCoarse = stats.terrainCoarse;
        sample.canopyCacheHits = stats.canopyCacheHits;
        sample.canopyCacheMisses = stats.canopyCacheMisses;
        sample.canopyCacheBytes = stats.canopyCacheBytes;
        sample.canopyNear = stats.canopyNear;
        sample.canopyMid = stats.canopyMid;
        sample.canopyFar = stats.canopyFar;
        sample.cameraEye = camera.eye();
        sample.cameraTarget = camera.strategy || cinematic ? camera.target : camera.pos + camera.forward();
        sample.fogVisible = visibility ? visibility->visibleCount : 0;
        sample.fogExplored = visibility ? visibility->exploredCount : 0;
        sample.fogRejected = stats.fogRejected;
        sample.fogUpdate = fogUpdateMs;
        sample.trace = stats.trace;
        if (o.traceRender) {
            sample.queryBegin = milliseconds(cpuStart, queryBeginEnd);
            sample.queryEnd = milliseconds(hudEnd, cpuEnd);
            sample.hud = milliseconds(renderEnd, hudEnd);
        }
        sample.regionTests = stats.regionTests;
        sample.regionsRejected = stats.regionsRejected;
        sample.objectCandidates = stats.objectCandidates;
        sample.objectsRegionSkipped = stats.objectsRegionSkipped;
        sample.runSeconds = elapsed;
        sample.measuredSeconds = measuring ? milliseconds(firstMeasured, start) / 1000.0 : 0;
        sample.startInterval = realDt * 1000;
        sample.loopGap = loopGap;
        sample.events = milliseconds(eventsStart, eventsEnd);
        sample.update = milliseconds(eventsEnd, updateEnd);
        sample.queries = milliseconds(updateEnd, cpuStart);
        sample.glCheck = o.frameGlChecks ? milliseconds(cpuEnd, checkEnd) : 0;
        sample.swap = milliseconds(swapStart, workEnd);
        sample.other = sample.work - sample.events - sample.update - sample.queries - sample.cpu -
                       sample.glCheck - sample.swap;
        sample.post = milliseconds(end, postEnd);
        sample.title = titleMs;
        sample.titleUpdated = titleUpdated;
        sample.storageGrew = storageGrew;
        previousLoopEnd = postEnd;
        ++frame;
    }
    if (firstMeasured != Clock::time_point{})
        measuredWall = milliseconds(firstMeasured, Clock::now()) / 1000.0;
    if (!o.seconds)
        finished = (limit == 0 || frame >= limit);
    // Resolve outstanding queries only after measurement, never blocking each frame.
    for (auto [id, ms] : renderer.collectGPU(true))
        if (id < int(samples.size()))
            samples[id].gpu = ms;
    if (o.resizeTest && !o.benchmark && (resizes < 2 || toggles != 2))
        throw std::runtime_error("Window/input integration test failed");
    saveReport(o, renderer, samples, frame, resizes, toggles, finished, measuredWall, actualWarmup,
               pacer, visibility.get());
    renderer.gl.check("Shutdown");
    return (o.benchmark && !finished) ? 2 : 0;
}
} // namespace pl
int main(int argc, char **argv) {
    try {
        return pl::run(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "PLEIADE ERROR: " << e.what() << '\n';
        std::ofstream("pleiade-error.txt") << e.what() << '\n';
        return 1;
    }
}
