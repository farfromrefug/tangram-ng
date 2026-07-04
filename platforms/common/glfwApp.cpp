#include "glfwApp.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stl.h"

#ifdef TANGRAM_WINDOWS
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#endif // TANGRAM_WINDOWS

#define GLFW_INCLUDE_GLEXT
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <atomic>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "gl.h"

#include "util/elevationManager.h"
#include "util/asyncWorker.h"

#ifndef BUILD_NUM_STRING
#define BUILD_NUM_STRING ""
#endif

void TANGRAM_WakeEventLoop() { glfwPostEmptyEvent(); }

namespace Tangram {

namespace GlfwApp {

// Forward-declare our callback functions.
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorMoveCallback(GLFWwindow* window, double x, double y);
void scrollCallback(GLFWwindow* window, double scrollx, double scrolly);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void dropCallback(GLFWwindow* window, int count, const char** paths);
void framebufferResizeCallback(GLFWwindow* window, int fWidth, int fHeight);

// Forward-declare GUI functions.
void showDebugFlagsGUI();
void showViewportGUI();
void showSceneGUI();
void showMarkerGUI();
void showMapSourcesGUI();
void showExamplesGUI();

constexpr double double_tap_time = 0.5; // seconds
constexpr double scroll_span_multiplier = 0.05; // scaling for zoom and rotation
constexpr double scroll_distance_multiplier = 5.0; // scaling for shove
constexpr double single_tap_time = 0.25; //seconds (to avoid a long press being considered as a tap)

std::string sceneFile = "scene.yaml";
std::string sceneYaml;
//std::string apiKey;

bool markerUseStylingPath = true;
std::string markerStylingPath = "layers.map-marker.draw.marker";
std::string markerStylingString = R"RAW(
style: text
text_source: "function() { return 'MARKER'; }"
font:
    family: global.font_sans
    size: 12px
    fill: black
    stroke: { color: white, width: 4 }
)RAW";
std::string polylineStyle = "{ style: lines, interactive: true, color: red, width: 20px, order: 5000 }";


GLFWwindow* main_window = nullptr;
Tangram::Map* map = nullptr;
int width = 800;
int height = 600;
float density = 1.0;
float pixel_scale = 1.0;
bool recreate_context = false;

bool was_panning = false;
double last_time_released = -double_tap_time; // First click should never trigger a double tap
double last_time_pressed = 0.0;
double last_time_moved = 0.0;
double last_x_down = 0.0;
double last_y_down = 0.0;
double last_x_velocity = 0.0;
double last_y_velocity = 0.0;

bool wireframe_mode = false;
bool show_gui = true;
bool load_async = true;
bool add_point_marker_on_click = false;
bool add_polyline_marker_on_click = false;
bool point_markers_position_clipped = false;

struct PointMarker {
    MarkerID markerId;
    LngLat coordinates;
};

std::vector<PointMarker> point_markers;

Tangram::MarkerID polyline_marker = 0;
std::vector<Tangram::LngLat> polyline_marker_coordinates;

Tangram::MarkerID pickResultMarker = 0;

std::vector<SceneUpdate> sceneUpdates;
//const char* apiKeyScenePath = "global.sdk_api_key";

// ---- Map Sources data ----
struct MapSource {
    std::string key;
    std::string title;
    bool layer = false;
    bool archived = false;
};

std::vector<MapSource> availableSources;
std::vector<std::string> activeSources;
bool sourcesLoaded = false;

// ---- Bookmark data ----
struct Bookmark {
    std::string name;
    double lng = 0.0;
    double lat = 0.0;
    MarkerID markerId = 0;
};
std::vector<Bookmark> bookmarks;
char bookmarkNameBuf[128] = "My Place";

// ---- Route data ----
struct RouteWaypoint {
    double lng = 0.0;
    double lat = 0.0;
    std::string label;
};
std::vector<RouteWaypoint> routeWaypoints;
Tangram::MarkerID routePolylineMarker = 0;
bool addRouteWaypointOnClick = false;

// ---- Search results data ----
struct SearchResult {
    std::string name;
    double lng = 0.0;
    double lat = 0.0;
    MarkerID markerId = 0;
};
std::vector<SearchResult> searchResults;
char searchQueryBuf[256] = "";
bool localSearchActive = false;

// ---- 3D terrain state ----
bool terrain3dEnabled = false;

// ---- Selection highlight ----
char selectedOsmIdBuf[64] = "";

// ---- Drawn sources (custom GeoJSON) ----
struct DrawnLayer {
    std::string sourceName;
    std::string geojson;
    MarkerID markerId = 0;
};
std::vector<DrawnLayer> drawnLayers;
char drawnLayerNameBuf[64] = "my-layer";
char drawnLayerGeojsonBuf[4096] = R"({"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[0,0]},"properties":{"name":"Origin"}}]})";

// Parse a simple mapsources YAML file and populate availableSources.
// Only handles the flat key: / title: / layer: / archived: structure.
static void parseMapsourcesFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) { return; }

    MapSource current;
    bool inEntry = false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') { continue; }
        // A top-level key (no leading space, ends with ':')
        if (line[0] != ' ' && line[0] != '\t') {
            if (inEntry && !current.key.empty()) {
                availableSources.push_back(current);
            }
            current = MapSource{};
            inEntry = false;
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                current.key = line.substr(0, colon);
                inEntry = true;
            }
        } else if (inEntry) {
            // Strip leading whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) { continue; }
            std::string trimmed = line.substr(start);
            auto colon = trimmed.find(':');
            if (colon == std::string::npos) { continue; }
            std::string key = trimmed.substr(0, colon);
            std::string value;
            if (colon + 1 < trimmed.size()) {
                size_t vs = trimmed.find_first_not_of(" \t", colon + 1);
                if (vs != std::string::npos) { value = trimmed.substr(vs); }
            }
            if (key == "title") { current.title = value; }
            else if (key == "layer") { current.layer = (value == "true"); }
            else if (key == "archived") { current.archived = (value == "true"); }
        }
    }
    if (inEntry && !current.key.empty()) {
        availableSources.push_back(current);
    }
}

void loadSceneFile(bool setPosition, std::vector<SceneUpdate> updates) {

    for (auto& update : updates) {
        bool found = false;
        for (auto& prev : sceneUpdates) {
            if (update.path == prev.path) {
                prev = update;
                found = true;
                break;
            }
        }
        if (!found) { sceneUpdates.push_back(update); }
    }
    SceneOptions options(sceneYaml, Url(sceneFile), setPosition, sceneUpdates);
    map->loadScene(std::move(options), load_async);
}

void parseArgs(int argc, char* argv[]) {
    // Load file from command line, if given.
    int argi = 0;
    while (++argi < argc) {
        if (strcmp(argv[argi - 1], "-f") == 0) {
            sceneFile = std::string(argv[argi]);
            LOG("File from command line: %s\n", argv[argi]);
            break;
        }
        if (strcmp(argv[argi - 1], "-s") == 0) {

            if (argi+1 < argc) {
                sceneYaml = std::string(argv[argi]);
                sceneFile = std::string(argv[argi+1]);
                LOG("Yaml from command line: %s, resource path: %s\n",
                    sceneYaml.c_str(), sceneFile.c_str());
            } else {
                LOG("-s options requires YAML string and resource path");
                exit(1);
            }
            break;
        }
    }
}

void setScene(const std::string& _path, const std::string& _yaml) {
    sceneFile = _path;
    sceneYaml = _yaml;
}

void create(std::unique_ptr<Platform> p, int w, int h) {

    width = w;
    height = h;

    if (!glfwInit()) {
        assert(false);
        return;
    }

    //char* nextzenApiKeyEnvVar = getenv("NEXTZEN_API_KEY");
    //if (nextzenApiKeyEnvVar && strlen(nextzenApiKeyEnvVar) > 0) {
    //    apiKey = nextzenApiKeyEnvVar;
    //}
    //
    //if (!apiKey.empty()) {
    //    sceneUpdates.push_back(SceneUpdate(apiKeyScenePath, apiKey));
    //}

    // Setup tangram
    if (!map) {
        map = new Tangram::Map(std::move(p));
    }

    // Build a version string for the window title.
    char versionString[256] = { 0 };
    std::snprintf(versionString, sizeof(versionString), "Tangram ES %d.%d.%d " BUILD_NUM_STRING,
        TANGRAM_VERSION_MAJOR, TANGRAM_VERSION_MINOR, TANGRAM_VERSION_PATCH);

    const char* glsl_version = "#version 120";

    // Create a windowed mode window and its OpenGL context
    glfwWindowHint(GLFW_SAMPLES, 2);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    if (!main_window) {
        main_window = glfwCreateWindow(width, height, versionString, NULL, NULL);
    }
    if (!main_window) {
        glfwTerminate();
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* glfwOffscreen = glfwCreateWindow(100, 100, "Offscreen", NULL, main_window);

    auto offscreenWorker = std::make_unique<AsyncWorker>("Ascend offscreen GL worker");
    offscreenWorker->enqueue([=](){ glfwMakeContextCurrent(glfwOffscreen); });
    ElevationManager::offscreenWorker = std::move(offscreenWorker);

    // Make the main_window's context current
    glfwMakeContextCurrent(main_window);
    glfwSwapInterval(1); // Enable vsync

#ifdef TANGRAM_WINDOWS
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
#endif

    // Set input callbacks
    glfwSetFramebufferSizeCallback(main_window, framebufferResizeCallback);
    glfwSetMouseButtonCallback(main_window, mouseButtonCallback);
    glfwSetCursorPosCallback(main_window, cursorMoveCallback);
    glfwSetScrollCallback(main_window, scrollCallback);
    glfwSetKeyCallback(main_window, keyCallback);
    glfwSetDropCallback(main_window, dropCallback);

    glfwSetCharCallback(main_window, ImGui_ImplGlfw_CharCallback);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    float dpi = std::max(mode->width, mode->height)/11.2f;
    map->setPixelScale(std::max(1.f, dpi/150.f));

    // Setup graphics
    map->setupGL();
    int fWidth = 0, fHeight = 0;
    glfwGetFramebufferSize(main_window, &fWidth, &fHeight);
    framebufferResizeCallback(main_window, fWidth, fHeight);

    // Setup ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    ImGui_ImplGlfw_InitForOpenGL(main_window, false);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Setup style
    ImGui::StyleColorsDark();

}

void run() {

    loadSceneFile(true);

    double lastTime = glfwGetTime();

    // Loop until the user closes the window
    while (!glfwWindowShouldClose(main_window)) {

        if(show_gui) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Create ImGui interface.
            // ImGui::ShowDemoWindow();
            showSceneGUI();
            showViewportGUI();
            showMarkerGUI();
            showDebugFlagsGUI();
            showMapSourcesGUI();
            showExamplesGUI();
        }
        double currentTime = glfwGetTime();
        double delta = currentTime - lastTime;
        lastTime = currentTime;

        // Render
        /*bool mapdirty =*/ map->getPlatform().notifyRender();
        MapState state = map->update(delta);
        if (state.isAnimating()) {
            map->getPlatform().requestRender();
        }

        const bool wireframe = wireframe_mode;
        if(wireframe) {
            glPolygonMode(GL_FRONT, GL_LINE);
            glPolygonMode(GL_BACK, GL_LINE);
        }
        map->render();
        if(wireframe) {
            glPolygonMode(GL_FRONT, GL_FILL);
            glPolygonMode(GL_BACK, GL_FILL);
        }

        if (show_gui) {
            // Render ImGui interface.
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        // Swap front and back buffers
        glfwSwapBuffers(main_window);

        // Poll for and process events
        if (map->getPlatform().isContinuousRendering()) {
            glfwPollEvents();
        } else {
            glfwWaitEvents();
        }
    }
}

void stop(int) {
    if (!glfwWindowShouldClose(main_window)) {
        logMsg("shutdown\n");
        glfwSetWindowShouldClose(main_window, 1);
        glfwPostEmptyEvent();
    } else {
        logMsg("killed!\n");
        exit(1);
    }
}

void destroy() {
    if (map) {
        delete map;
        map = nullptr;
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    if (main_window) {
        glfwDestroyWindow(main_window);
        main_window = nullptr;
    }
    glfwTerminate();
}

template<typename T>
static constexpr T clamp(T val, T min, T max) {
    return val > max ? max : val < min ? min : val;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {

    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) {
        return; // Imgui is handling this event.
    }

    if (button != GLFW_MOUSE_BUTTON_1) {
        return; // This event is for a mouse button that we don't care about
    }

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    x *= density;
    y *= density;
    double time = glfwGetTime();

    if (was_panning && action == GLFW_RELEASE) {
        was_panning = false;
        auto vx = clamp(last_x_velocity, -2000.0, 2000.0);
        auto vy = clamp(last_y_velocity, -2000.0, 2000.0);
        map->handleFlingGesture(x, y, vx, vy);
        return; // Clicks with movement don't count as taps, so stop here
    }

    if (action == GLFW_PRESS) {
        map->handlePanGesture(0.0f, 0.0f, 0.0f, 0.0f);
        last_x_down = x;
        last_y_down = y;
        last_time_pressed = time;
        return;
    }

    if ((time - last_time_released) < double_tap_time) {
        // Double tap recognized
        const float duration = 0.5f;
        Tangram::LngLat tapped, current;
        map->screenPositionToLngLat(x, y, &tapped.longitude, &tapped.latitude);
        map->getPosition(current.longitude, current.latitude);
        auto pos = map->getCameraPosition();
        pos.zoom += 1.f;
        pos.longitude = tapped.longitude;
        pos.latitude = tapped.latitude;

        map->setCameraPositionEased(pos, duration, EaseType::quint);
    } else if ((time - last_time_pressed) < single_tap_time) {
        // Single tap recognized
        Tangram::LngLat location;
        map->screenPositionToLngLat(x, y, &location.longitude, &location.latitude);
        double xx, yy;
        map->lngLatToScreenPosition(location.longitude, location.latitude, &xx, &yy);

        logMsg("------\n");
        logMsg("LngLat: %f, %f\n", location.longitude, location.latitude);
        logMsg("Clicked:  %f, %f\n", x, y);
        logMsg("Remapped: %f, %f\n", xx, yy);

        map->pickLabelAt(x, y, [&](const LabelPickResult* result) {
            if (result == nullptr) {
                logMsg("Pick Label result is null.\n");
                return;
            }
            if (pickResultMarker == 0) {
                pickResultMarker = map->markerAdd();
                map->markerSetStylingFromPath(pickResultMarker, "layers.pick-result.draw.pick-marker");
            }
            map->markerSetPoint(pickResultMarker, result->coordinates);
            logMsg("Pick label result:\n");
            for (const auto& item : result->touchItem.properties->items()) {
                logMsg("  %s = %s\n", item.key.c_str(), Properties::asString(item.value).c_str());
            }
        });

        if (add_point_marker_on_click) {
            auto marker = map->markerAdd();
            map->markerSetPoint(marker, location);
            if (markerUseStylingPath) {
                map->markerSetStylingFromPath(marker, markerStylingPath.c_str());
            } else {
                map->markerSetStylingFromString(marker, markerStylingString.c_str());
            }

            point_markers.push_back({ marker, location });
        }

        if (add_polyline_marker_on_click) {
            if (polyline_marker_coordinates.empty()) {
                polyline_marker = map->markerAdd();
                map->markerSetStylingFromString(polyline_marker, polylineStyle.c_str());
            }
            polyline_marker_coordinates.push_back(location);
            map->markerSetPolyline(polyline_marker, polyline_marker_coordinates.data(), polyline_marker_coordinates.size());
        }

        if (addRouteWaypointOnClick) {
            char label[32];
            std::snprintf(label, sizeof(label), "WP%d", (int)routeWaypoints.size() + 1);
            routeWaypoints.push_back({ location.longitude, location.latitude, label });
            if (routePolylineMarker == 0) {
                routePolylineMarker = map->markerAdd();
                map->markerSetStylingFromString(routePolylineMarker,
                    "{ style: lines, color: blue, width: 6px, order: 5001 }");
            }
            std::vector<Tangram::LngLat> pts;
            for (const auto& wp : routeWaypoints) { pts.push_back({ wp.lng, wp.lat }); }
            map->markerSetPolyline(routePolylineMarker, pts.data(), pts.size());
        }

        map->getPlatform().requestRender();
    }

    last_time_released = time;

}

void cursorMoveCallback(GLFWwindow* window, double x, double y) {

    if (ImGui::GetIO().WantCaptureMouse) {
        return; // Imgui is handling this event.
    }

    x *= density;
    y *= density;

    int action = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1);
    double time = glfwGetTime();

    if (action == GLFW_PRESS) {

        if (was_panning) {
            map->handlePanGesture(last_x_down, last_y_down, x, y);
        }

        was_panning = true;
        last_x_velocity = (x - last_x_down) / (time - last_time_moved);
        last_y_velocity = (y - last_y_down) / (time - last_time_moved);
        last_x_down = x;
        last_y_down = y;

    }

    last_time_moved = time;

}

void scrollCallback(GLFWwindow* window, double scrollx, double scrolly) {

    ImGui_ImplGlfw_ScrollCallback(window, scrollx, scrolly);
    if (ImGui::GetIO().WantCaptureMouse) {
        return; // Imgui is handling this event.
    }

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    x *= density;
    y *= density;

    bool rotating = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    bool shoving = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    if (shoving) {
        map->handleShoveGesture(scroll_distance_multiplier * scrolly);
    } else if (rotating) {
        map->handleRotateGesture(x, y, scroll_span_multiplier * scrolly);
    } else {
        map->handlePinchGesture(x, y, 1.0 + scroll_span_multiplier * scrolly, 0.f);
    }

}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return; // Imgui is handling this event.
    }

    CameraPosition camera = map->getCameraPosition();

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_A:
                load_async = !load_async;
                LOG("Toggle async load: %d", load_async);
                break;
            case GLFW_KEY_D:
                show_gui = !show_gui;
                break;
            case GLFW_KEY_BACKSPACE:
                recreate_context = true;
                break;
            case GLFW_KEY_R:
                loadSceneFile();
                break;
            case GLFW_KEY_Z: {
                auto pos = map->getCameraPosition();
                pos.zoom += 1.f;
                map->setCameraPositionEased(pos, 1.5f);
                break;
            }
            case GLFW_KEY_N: {
                auto pos = map->getCameraPosition();
                pos.rotation = 0.f;
                map->setCameraPositionEased(pos, 1.f);
                break;
            }
            case GLFW_KEY_S:
                if (pixel_scale == 1.0) {
                    pixel_scale = 2.0;
                } else if (pixel_scale == 2.0) {
                    pixel_scale = 0.75;
                } else {
                    pixel_scale = 1.0;
                }
                map->setPixelScale(pixel_scale);
                break;
            case GLFW_KEY_P:
                loadSceneFile(false, {SceneUpdate{"cameras", "{ main_camera: { type: perspective } }"}});
                break;
            case GLFW_KEY_I:
                loadSceneFile(false, {SceneUpdate{"cameras", "{ main_camera: { type: isometric } }"}});
                break;
            case GLFW_KEY_M:
                map->loadSceneYamlAsync("{ scene: { background: { color: red } } }", std::string(""));
                break;
            case GLFW_KEY_G:
                {
                    static bool geoJSON = false;
                    if (!geoJSON) {
                        loadSceneFile(false,
                                      { SceneUpdate{"sources.osm.type", "GeoJSON"},
                                        SceneUpdate{"sources.osm.url", "https://tile.mapzen.com/mapzen/vector/v1/all/{z}/{x}/{y}.json"}});
                    } else {
                        loadSceneFile(false,
                                      { SceneUpdate{"sources.osm.type", "MVT"},
                                        SceneUpdate{"sources.osm.url", "https://tile.mapzen.com/mapzen/vector/v1/all/{z}/{x}/{y}.mvt"}});
                    }
                    geoJSON = !geoJSON;
                }
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_F1:
                map->setPosition(-74.00976419448854, 40.70532700869127);
                map->setZoom(16);
                break;
            case GLFW_KEY_F2:
                map->setPosition(8.82, 53.08);
                map->setZoom(14);
                break;
            case GLFW_KEY_F3:
                camera.longitude = 8.82;
                camera.latitude = 53.08;
                camera.zoom = 16.f;
                map->flyTo(camera, -1.f, 2.0);
                break;
            case GLFW_KEY_F4:
                camera.longitude = 8.82;
                camera.latitude = 53.08;
                camera.zoom = 10.f;
                map->flyTo(camera, -1.f, 2.5);
                break;
            case GLFW_KEY_F5:
                camera.longitude = -74.00976419448854;
                camera.latitude = 40.70532700869127;
                camera.zoom = 16.f;
                map->flyTo(camera, 4.);
                break;
            case GLFW_KEY_F6:
                camera.longitude = -122.41;
                camera.latitude = 37.7749;
                camera.zoom = 16.f;
                map->flyTo(camera, -1.f, 4.0);
                break;
            case GLFW_KEY_F7:
                camera.longitude = 139.839478;
                camera.latitude = 35.652832;
                camera.zoom = 16.f;
                map->flyTo(camera, -1.f, 1.0);
                break;
            case GLFW_KEY_F8: // Beijing
                map->setCameraPosition({116.39703, 39.91006, 12.5});
                break;
            case GLFW_KEY_F9: // Bangkok
                map->setCameraPosition({100.49216, 13.7556, 12.5});
                break;
            case GLFW_KEY_F10: // Dhaka
                map->setCameraPosition({90.40166, 23.72909, 14.5});
                break;
            case GLFW_KEY_F11: // Tehran
                map->setCameraPosition({51.42086, 35.7409, 13.5});
                break;
            case GLFW_KEY_W:
                map->onMemoryWarning();
                break;
            default:
                break;
        }
    }
}

void dropCallback(GLFWwindow* window, int count, const char** paths) {

    sceneFile = "file://" + std::string(paths[0]);
    sceneYaml.clear();

    loadSceneFile();
}

void framebufferResizeCallback(GLFWwindow* window, int fWidth, int fHeight) {

    int wWidth = 0, wHeight = 0;
    glfwGetWindowSize(main_window, &wWidth, &wHeight);
#ifdef ENABLE_DYNAMIC_DENSITY
    float new_density = (float)fWidth / (float)wWidth;
    if (new_density != density) {
        recreate_context = true;
        density = new_density;
    }
    map->setPixelScale(density);
#endif
    map->resize(fWidth, fHeight);
}

void showSceneGUI() {
    if (ImGui::CollapsingHeader("Scene")) {
        if (ImGui::InputText("Scene URL", &sceneFile, ImGuiInputTextFlags_EnterReturnsTrue)) {
            loadSceneFile();
        }
        //if (ImGui::InputText("API key", &apiKey, ImGuiInputTextFlags_EnterReturnsTrue)) {
        //    loadSceneFile(false, {SceneUpdate{apiKeyScenePath, apiKey}});
        //}
        if (ImGui::Button("Reload Scene")) {
            loadSceneFile();
        }
    }
}

void showMarkerGUI() {
    if (ImGui::CollapsingHeader("Markers")) {
        ImGui::Checkbox("Add point markers on click", &add_point_marker_on_click);
        if (ImGui::RadioButton("Use Styling Path", markerUseStylingPath)) { markerUseStylingPath = true; }
        if (markerUseStylingPath) {
            ImGui::InputText("Path", &markerStylingPath);
        }
        if (ImGui::RadioButton("Use Styling String", !markerUseStylingPath)) { markerUseStylingPath = false; }
        if (!markerUseStylingPath) {
            ImGui::InputTextMultiline("String", &markerStylingString);
        }
        if (ImGui::Button("Clear point markers")) {
            for (const auto marker : point_markers) {
                map->markerRemove(marker.markerId);
            }
            point_markers.clear();
        }

        ImGui::Checkbox("Add polyline marker points on click", &add_polyline_marker_on_click);
        if (ImGui::Button("Clear polyline marker")) {
            if (!polyline_marker_coordinates.empty()) {
                map->markerRemove(polyline_marker);
                polyline_marker_coordinates.clear();
            }
        }

        ImGui::Checkbox("Point markers use clipped position", &point_markers_position_clipped);
        if (point_markers_position_clipped) {
            // Move all point markers to "clipped" positions.
            for (const auto& marker : point_markers) {
                double screenClipped[2];
                map->lngLatToScreenPosition(marker.coordinates.longitude, marker.coordinates.latitude, &screenClipped[0], &screenClipped[1], true);
                LngLat lngLatClipped;
                map->screenPositionToLngLat(screenClipped[0], screenClipped[1], &lngLatClipped.longitude, &lngLatClipped.latitude);
                map->markerSetPoint(marker.markerId, lngLatClipped);
            }

            // Display coordinates for last marker.
            if (!point_markers.empty()) {
                auto& last_marker = point_markers.back();
                double screenPosition[2];
                map->lngLatToScreenPosition(last_marker.coordinates.longitude, last_marker.coordinates.latitude, &screenPosition[0], &screenPosition[1]);
                float screenPositionFloat[2] = {static_cast<float>(screenPosition[0]), static_cast<float>(screenPosition[1])};
                ImGui::InputFloat2("Last Marker Screen", screenPositionFloat, "%.5f", ImGuiInputTextFlags_ReadOnly);
                double screenClipped[2];
                map->lngLatToScreenPosition(last_marker.coordinates.longitude, last_marker.coordinates.latitude, &screenClipped[0], &screenClipped[1], true);
                float screenClippedFloat[2] = {static_cast<float>(screenClipped[0]), static_cast<float>(screenClipped[1])};
                ImGui::InputFloat2("Last Marker Clipped", screenClippedFloat, "%.5f", ImGuiInputTextFlags_ReadOnly);
            }
        }
    }
}

void showViewportGUI() {
    if (ImGui::CollapsingHeader("Viewport")) {
        CameraPosition camera = map->getCameraPosition();
        float lngLatZoom[3] = {static_cast<float>(camera.longitude), static_cast<float>(camera.latitude), camera.zoom};
        if (ImGui::InputFloat3("Lng/Lat/Zoom", lngLatZoom, "%.5f", ImGuiInputTextFlags_EnterReturnsTrue)) {
            camera.longitude = lngLatZoom[0];
            camera.latitude = lngLatZoom[1];
            camera.zoom = lngLatZoom[2];
            map->setCameraPosition(camera);
        }
        if (ImGui::SliderAngle("Tilt", &camera.tilt, 0.f, 90.f)) {
            map->setCameraPosition(camera);
        }
        if (ImGui::SliderAngle("Rotation", &camera.rotation, 0.f, 360.f)) {
            map->setCameraPosition(camera);
        }
        EdgePadding padding = map->getPadding();
        if (ImGui::InputInt4("Left/Top/Right/Bottom", &padding.left)) {
            map->setPadding(padding);
        }
    }
}

void showDebugFlagsGUI() {
    if (ImGui::CollapsingHeader("Debug Flags")) {
        bool flag;
        flag = getDebugFlag(DebugFlags::freeze_tiles);
        if (ImGui::Checkbox("Freeze Tiles", &flag)) {
            setDebugFlag(DebugFlags::freeze_tiles, flag);
        }
        flag = getDebugFlag(DebugFlags::proxy_colors);
        if (ImGui::Checkbox("Recolor Proxy Tiles", &flag)) {
            setDebugFlag(DebugFlags::proxy_colors, flag);
        }
        flag = getDebugFlag(DebugFlags::tile_bounds);
        if (ImGui::Checkbox("Show Tile Bounds", &flag)) {
            setDebugFlag(DebugFlags::tile_bounds, flag);
        }
        flag = getDebugFlag(DebugFlags::labels);
        if (ImGui::Checkbox("Show Label Debug Info", &flag)) {
            setDebugFlag(DebugFlags::labels, flag);
        }
        flag = getDebugFlag(DebugFlags::tangram_infos);
        if (ImGui::Checkbox("Show Map Info", &flag)) {
            setDebugFlag(DebugFlags::tangram_infos, flag);
        }
        flag = getDebugFlag(DebugFlags::draw_all_labels);
        if (ImGui::Checkbox("Show All Labels", &flag)) {
            setDebugFlag(DebugFlags::draw_all_labels, flag);
        }
        flag = getDebugFlag(DebugFlags::tangram_stats);
        if (ImGui::Checkbox("Show Frame Stats", &flag)) {
            setDebugFlag(DebugFlags::tangram_stats, flag);
        }
        flag = getDebugFlag(DebugFlags::selection_buffer);
        if (ImGui::Checkbox("Show Selection Buffer", &flag)) {
            setDebugFlag(DebugFlags::selection_buffer, flag);
        }
        ImGui::Checkbox("Wireframe Mode", &wireframe_mode);
    }
}

void showMapSourcesGUI() {
    if (!sourcesLoaded) {
        // Try to load from the res directory relative to cwd
        parseMapsourcesFile("res/mapsources.default.yaml");
        if (availableSources.empty()) {
            // Fallback: try same directory as scene file
            parseMapsourcesFile("mapsources.default.yaml");
        }
        sourcesLoaded = true;
    }

    if (ImGui::CollapsingHeader("Map Sources")) {
        // Button to open add-source popup
        if (ImGui::Button("Add Source...")) {
            ImGui::OpenPopup("select_source_popup");
        }

        if (ImGui::BeginPopup("select_source_popup")) {
            ImGui::Text("Select a source to add:");
            ImGui::Separator();
            for (const auto& src : availableSources) {
                bool alreadyActive = std::find(activeSources.begin(), activeSources.end(), src.key) != activeSources.end();
                if (alreadyActive) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f)); }
                std::string label = src.title.empty() ? src.key : src.title;
                if (src.layer) { label += " [overlay]"; }
                if (src.archived) { label += " [archived]"; }
                if (ImGui::Selectable(label.c_str(), false, alreadyActive ? ImGuiSelectableFlags_Disabled : 0)) {
                    activeSources.push_back(src.key);
                    // Apply a scene update so the source URL-template is activated.
                    // The mapsources system uses global vars; here we signal the key name.
                    loadSceneFile(false, { SceneUpdate{ "global.active_source_" + src.key, "true" } });
                    ImGui::CloseCurrentPopup();
                }
                if (alreadyActive) { ImGui::PopStyleColor(); }
            }
            ImGui::EndPopup();
        }

        // List active sources with remove button
        if (!activeSources.empty()) {
            ImGui::Text("Active sources:");
            std::string toRemove;
            for (const auto& key : activeSources) {
                ImGui::BulletText("%s", key.c_str());
                ImGui::SameLine();
                std::string btnLabel = "Remove##" + key;
                if (ImGui::SmallButton(btnLabel.c_str())) {
                    toRemove = key;
                }
            }
            if (!toRemove.empty()) {
                activeSources.erase(std::remove(activeSources.begin(), activeSources.end(), toRemove), activeSources.end());
                loadSceneFile(false, { SceneUpdate{ "global.active_source_" + toRemove, "false" } });
            }
        } else {
            ImGui::TextDisabled("No sources added yet.");
        }
    }
}

void showExamplesGUI() {
    if (!ImGui::CollapsingHeader("Examples")) { return; }

    // ---- 1. Drawn sources / layers ----
    if (ImGui::TreeNode("Drawn Sources / Layers")) {
        ImGui::InputText("Source name", drawnLayerNameBuf, sizeof(drawnLayerNameBuf));
        ImGui::InputTextMultiline("GeoJSON", drawnLayerGeojsonBuf, sizeof(drawnLayerGeojsonBuf), ImVec2(-1, 80));
        if (ImGui::Button("Add layer")) {
            std::string srcName = drawnLayerNameBuf;
            std::string geojson = drawnLayerGeojsonBuf;
            // Add source + layer via SceneUpdates
            loadSceneFile(false, {
                SceneUpdate{ "sources." + srcName + ".type", "GeoJSON" },
                SceneUpdate{ "sources." + srcName + ".data", geojson },
                SceneUpdate{ "layers." + srcName + ".data.source", srcName },
                SceneUpdate{ "layers." + srcName + ".draw.points.color", "red" },
                SceneUpdate{ "layers." + srcName + ".draw.points.size", "12px" },
                SceneUpdate{ "layers." + srcName + ".draw.points.order", "5000" }
            });
            drawnLayers.push_back({ srcName, geojson, 0 });
        }
        ImGui::Separator();
        std::string toRemoveLayer;
        for (const auto& dl : drawnLayers) {
            ImGui::BulletText("%s", dl.sourceName.c_str());
            ImGui::SameLine();
            std::string btnId = "Remove##dl_" + dl.sourceName;
            if (ImGui::SmallButton(btnId.c_str())) { toRemoveLayer = dl.sourceName; }
        }
        if (!toRemoveLayer.empty()) {
            drawnLayers.erase(std::remove_if(drawnLayers.begin(), drawnLayers.end(),
                [&](const DrawnLayer& d) { return d.sourceName == toRemoveLayer; }), drawnLayers.end());
            // Disable the layer by removing draw rule
            loadSceneFile(false, {
                SceneUpdate{ "layers." + toRemoveLayer + ".draw.points.size", "0px" }
            });
        }
        ImGui::TreePop();
    }

    // ---- 2. Routes ----
    if (ImGui::TreeNode("Routes")) {
        ImGui::Checkbox("Add waypoint on map click", &addRouteWaypointOnClick);
        ImGui::Text("Waypoints: %d", (int)routeWaypoints.size());
        for (size_t i = 0; i < routeWaypoints.size(); ++i) {
            ImGui::BulletText("%s: %.5f, %.5f", routeWaypoints[i].label.c_str(),
                routeWaypoints[i].lng, routeWaypoints[i].lat);
        }
        if (ImGui::Button("Clear Route")) {
            routeWaypoints.clear();
            if (routePolylineMarker != 0) {
                map->markerRemove(routePolylineMarker);
                routePolylineMarker = 0;
            }
        }
        ImGui::TreePop();
    }

    // ---- 3. Local search in vector tiles ----
    if (ImGui::TreeNode("Local Search in Vector Tiles")) {
        ImGui::InputText("Query (feature name)", searchQueryBuf, sizeof(searchQueryBuf));
        if (ImGui::Button("Search at center")) {
            // Pick features near screen center
            localSearchActive = true;
            for (const auto& r : searchResults) {
                if (r.markerId != 0) { map->markerRemove(r.markerId); }
            }
            searchResults.clear();
            float cx = (float)(width * 0.5 * density);
            float cy = (float)(height * 0.5 * density);
            map->pickFeatureAt(cx, cy, [](const FeaturePickResult* result) {
                if (!result) { return; }
                for (const auto& item : result->properties->items()) {
                    std::string val = Properties::asString(item.value);
                    if (item.key == "name" && !val.empty()) {
                        SearchResult sr;
                        sr.name = val;
                        // Convert screen position to LngLat
                        map->screenPositionToLngLat(result->position[0], result->position[1],
                            &sr.lng, &sr.lat);
                        sr.markerId = map->markerAdd();
                        map->markerSetPoint(sr.markerId, { sr.lng, sr.lat });
                        map->markerSetStylingFromString(sr.markerId,
                            "{ style: text, text_source: function(){return 'RESULT';}, font: { size: 10px, fill: orange } }");
                        searchResults.push_back(sr);
                        break;
                    }
                }
            });
        }
        ImGui::TreePop();
    }

    // ---- 4. Search results on map ----
    if (ImGui::TreeNode("Search Results")) {
        ImGui::Text("Results: %d", (int)searchResults.size());
        std::string toRemoveSR;
        for (const auto& r : searchResults) {
            ImGui::BulletText("%s (%.4f, %.4f)", r.name.c_str(), r.lng, r.lat);
            ImGui::SameLine();
            std::string btnId = "Fly##sr_" + r.name;
            if (ImGui::SmallButton(btnId.c_str())) {
                map->setCameraPosition({ r.lng, r.lat, 16.f });
            }
            ImGui::SameLine();
            std::string removeId = "X##sr_" + r.name;
            if (ImGui::SmallButton(removeId.c_str())) { toRemoveSR = r.name; }
        }
        if (!toRemoveSR.empty()) {
            searchResults.erase(std::remove_if(searchResults.begin(), searchResults.end(),
                [&](const SearchResult& r) {
                    if (r.name == toRemoveSR) { map->markerRemove(r.markerId); return true; }
                    return false;
                }), searchResults.end());
        }
        if (ImGui::Button("Clear all results")) {
            for (const auto& r : searchResults) { if (r.markerId != 0) { map->markerRemove(r.markerId); } }
            searchResults.clear();
        }
        ImGui::TreePop();
    }

    // ---- 5. Bookmarks ----
    if (ImGui::TreeNode("Bookmarks")) {
        ImGui::InputText("Name", bookmarkNameBuf, sizeof(bookmarkNameBuf));
        if (ImGui::Button("Bookmark current view")) {
            auto cam = map->getCameraPosition();
            Bookmark bm;
            bm.name = bookmarkNameBuf;
            bm.lng = cam.longitude;
            bm.lat = cam.latitude;
            bm.markerId = map->markerAdd();
            map->markerSetPoint(bm.markerId, { bm.lng, bm.lat });
            std::string styling = "{ style: text, text_source: function(){return '"
                + bm.name + "';}, font: { size: 11px, fill: '#c040c0', stroke: { color: white, width: 3 } } }";
            map->markerSetStylingFromString(bm.markerId, styling.c_str());
            bookmarks.push_back(bm);
        }
        ImGui::Separator();
        std::string toRemoveBM;
        for (const auto& bm : bookmarks) {
            ImGui::BulletText("%s  (%.4f, %.4f)", bm.name.c_str(), bm.lng, bm.lat);
            ImGui::SameLine();
            std::string flyId = "Fly##bm_" + bm.name;
            if (ImGui::SmallButton(flyId.c_str())) {
                map->setCameraPosition({ bm.lng, bm.lat, 15.f });
            }
            ImGui::SameLine();
            std::string rmId = "X##bm_" + bm.name;
            if (ImGui::SmallButton(rmId.c_str())) { toRemoveBM = bm.name; }
        }
        if (!toRemoveBM.empty()) {
            bookmarks.erase(std::remove_if(bookmarks.begin(), bookmarks.end(),
                [&](const Bookmark& bm) {
                    if (bm.name == toRemoveBM) { map->markerRemove(bm.markerId); return true; }
                    return false;
                }), bookmarks.end());
        }
        ImGui::TreePop();
    }

    // ---- 6. 3D terrain ----
    if (ImGui::TreeNode("3D Terrain")) {
        ImGui::TextWrapped("Toggle 3D terrain rendering. Requires hillshade/elevation source.");
        if (ImGui::Checkbox("Enable 3D Terrain", &terrain3dEnabled)) {
            if (terrain3dEnabled) {
                loadSceneFile(false, {
                    SceneUpdate{ "import", "scenes/terrain-3d.yaml" },
                    SceneUpdate{ "global.show_land_polygons", "false" }
                });
            } else {
                loadSceneFile(false, {
                    SceneUpdate{ "global.show_land_polygons", "true" }
                });
            }
        }
        ImGui::TreePop();
    }

    // ---- 7. Custom shader rendering ----
    if (ImGui::TreeNode("Custom Shader Rendering")) {
        ImGui::TextWrapped("Activate slope/hillshade overlays via scene updates.");
        if (ImGui::Button("Enable Hillshade")) {
            loadSceneFile(false, {
                SceneUpdate{ "import", "scenes/hillshade.yaml" },
                SceneUpdate{ "global.show_hypsometric", "true" }
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Enable Slope Angle")) {
            loadSceneFile(false, {
                SceneUpdate{ "import", "scenes/slope-angle.yaml" },
                SceneUpdate{ "hillshade.shaders.uniforms.u_slope_angle_opacity", "0.75" }
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Enable Custom Slope")) {
            loadSceneFile(false, {
                SceneUpdate{ "import", "scenes/custom-slope-shading.yaml" },
                SceneUpdate{ "hillshade.shaders.uniforms.u_custom_slope_opacity", "0.75" }
            });
        }
        if (ImGui::Button("Disable All Overlays")) {
            loadSceneFile(false, {
                SceneUpdate{ "global.show_hypsometric", "false" },
                SceneUpdate{ "hillshade.shaders.uniforms.u_slope_angle_opacity", "0.0" },
                SceneUpdate{ "hillshade.shaders.uniforms.u_custom_slope_opacity", "0.0" }
            });
        }
        ImGui::TreePop();
    }

    // ---- 8. Selection highlighting ----
    if (ImGui::TreeNode("Selection Highlighting")) {
        ImGui::TextWrapped("Set global.selected_osm_id to highlight a feature. Click on the map to pick an OSM id.");
        ImGui::InputText("OSM ID", selectedOsmIdBuf, sizeof(selectedOsmIdBuf));
        if (ImGui::Button("Highlight Feature")) {
            loadSceneFile(false, {
                SceneUpdate{ "global.selected_osm_id", selectedOsmIdBuf }
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Highlight")) {
            selectedOsmIdBuf[0] = '\0';
            loadSceneFile(false, { SceneUpdate{ "global.selected_osm_id", "" } });
        }
        ImGui::TextDisabled("Tip: click a feature, check the log for its osm_id, then paste it above.");
        ImGui::TreePop();
    }
}

} // namespace GlfwApp

} // namespace Tangram
