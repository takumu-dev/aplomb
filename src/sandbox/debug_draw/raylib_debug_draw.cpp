#include "debug_draw/raylib_debug_draw.hpp"

#include "debug_draw/camera_view.hpp"
#include <aplomb/core/assertion.hpp>
#include <aplomb/core/transform.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Vec3.h>
#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int window_width = 1280;
constexpr int window_height = 720;
constexpr int frames_per_second = 60;

constexpr ::Color background{.r = 38, .g = 42, .b = 48, .a = 255};

constexpr int sphere_rings = 12;
constexpr int sphere_slices = 16;
constexpr int cylinder_slices = 16;

// Each part is lit by one light from above and to the side, with some ambient light so that the
// side facing away from it still shows its shape.
constexpr const char* vertex_shader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matNormal;
out vec3 fragNormal;
void main() {
    fragNormal = mat3(matNormal) * vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* fragment_shader = R"(#version 330
in vec3 fragNormal;
uniform vec4 colDiffuse;
out vec4 finalColor;
const vec3 to_light = normalize(vec3(0.4, 1.0, 0.3));
void main() {
    float diffuse = max(dot(normalize(fragNormal), to_light), 0.0);
    finalColor = vec4(colDiffuse.rgb * (0.35 + 0.65 * diffuse), colDiffuse.a);
}
)";

struct LineVertex {
    JPH::Vec3 position;
    JPH::Color color;
};

// A colour's red, green, blue and alpha, in the order Jolt lays them out.
std::array<std::uint8_t, 4> channels(JPH::Color color) {
    return std::bit_cast<std::array<std::uint8_t, 4>>(color);
}

::Color to_raylib(JPH::Color color) {
    const auto [red, green, blue, alpha] = channels(color);
    return {.r = red, .g = green, .b = blue, .a = alpha};
}

::Vector3 to_raylib(JPH::Vec3 vector) {
    return {.x = vector.GetX(), .y = vector.GetY(), .z = vector.GetZ()};
}

// raylib's matrices are column-major, but its fields are declared a row at a time.
::Matrix to_raylib(const JPH::Mat44& matrix) {
    return {
        .m0 = matrix(0, 0),
        .m4 = matrix(0, 1),
        .m8 = matrix(0, 2),
        .m12 = matrix(0, 3),
        .m1 = matrix(1, 0),
        .m5 = matrix(1, 1),
        .m9 = matrix(1, 2),
        .m13 = matrix(1, 3),
        .m2 = matrix(2, 0),
        .m6 = matrix(2, 1),
        .m10 = matrix(2, 2),
        .m14 = matrix(2, 3),
        .m3 = matrix(3, 0),
        .m7 = matrix(3, 1),
        .m11 = matrix(3, 2),
        .m15 = matrix(3, 3),
    };
}

JPH::Mat44 placement(const aplomb::Transform& transform) {
    return JPH::Mat44::sRotationTranslation(transform.rotation, transform.position);
}

// Draws a mesh of the shapes' unit size, scaled and placed by the matrix, in the material's shader.
void draw_mesh(const ::Mesh& mesh, const ::Material& material, const JPH::Mat44& matrix,
               JPH::Color color) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): raylib's maps are a C array
    material.maps[MATERIAL_MAP_DIFFUSE].color = to_raylib(color);
    DrawMesh(mesh, material, to_raylib(matrix));
}

} // namespace

struct RaylibDebugDraw::Resources {
    Resources() = default;
    Resources(const Resources&) = delete;
    Resources(Resources&&) = delete;
    Resources& operator=(const Resources&) = delete;
    Resources& operator=(Resources&&) = delete;

    ~Resources() {
        if (line_batch_elements > 0) {
            rlUnloadRenderBatch(line_batch);
        }
        UnloadMesh(cube);
        UnloadMesh(sphere);
        UnloadMesh(cylinder);
        UnloadMaterial(material);
        rlImGuiShutdown();
        CloseWindow();
    }

    // Draws lines gathered over the scene from a render batch of their own, a single vertex buffer
    // grown to hold them all, so that rlgl never splits them into several draws.
    void draw_lines(std::vector<LineVertex>& vertices) {
        if (vertices.empty()) {
            return;
        }
        // rlgl counts a batch's size in quads of four vertices, and keeps one quad spare.
        const int needed = static_cast<int>(vertices.size() / 4) + 2;
        if (needed > line_batch_elements) {
            if (line_batch_elements > 0) {
                rlUnloadRenderBatch(line_batch);
            }
            line_batch_elements = std::max(needed, line_batch_elements * 2);
            line_batch = rlLoadRenderBatch(1, line_batch_elements);
        }
        rlSetRenderBatchActive(&line_batch);
        rlBegin(RL_LINES);
        for (const LineVertex& vertex : vertices) {
            const auto [red, green, blue, alpha] = channels(vertex.color);
            rlColor4ub(red, green, blue, alpha);
            rlVertex3f(vertex.position.GetX(), vertex.position.GetY(), vertex.position.GetZ());
        }
        rlEnd();
        // Going back to rlgl's own batch draws this one.
        rlSetRenderBatchActive(nullptr);
        vertices.clear();
    }

    // Draws the lines, then the overlay lines over everything drawn before them.
    void draw_all_lines() {
        draw_lines(line_vertices);
        if (!overlay_vertices.empty()) {
            rlDisableDepthTest();
            draw_lines(overlay_vertices);
            rlEnableDepthTest();
        }
    }

    ::Material material{};
    ::Mesh cube{};
    ::Mesh sphere{};
    ::Mesh cylinder{};
    // Two for each line, its ends.
    std::vector<LineVertex> line_vertices;
    std::vector<LineVertex> overlay_vertices;
    rlRenderBatch line_batch{};
    int line_batch_elements = 0;
};

std::expected<RaylibDebugDraw, std::string> RaylibDebugDraw::open(const std::string& title) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(window_width, window_height, title.c_str());
    if (!IsWindowReady()) {
        return std::unexpected("the window couldn't be opened, or OpenGL 3.3 isn't available");
    }
    SetTargetFPS(frames_per_second);

    auto resources = std::make_unique<Resources>();
    rlImGuiSetup(true);
    // The panels' layout isn't worth a file beside wherever the sandbox was started.
    ImGui::GetIO().IniFilename = nullptr;

    resources->material = LoadMaterialDefault();
    resources->material.shader = LoadShaderFromMemory(vertex_shader, fragment_shader);
    // Unit shapes, which each draw scales: a box from -1 to 1 on each axis, a sphere of radius 1,
    // and a cylinder of radius 1 from 0 to 1 along Y.
    resources->cube = GenMeshCube(2.0F, 2.0F, 2.0F);
    resources->sphere = GenMeshSphere(1.0F, sphere_rings, sphere_slices);
    resources->cylinder = GenMeshCylinder(1.0F, 1.0F, cylinder_slices);

    return RaylibDebugDraw(std::move(resources));
}

RaylibDebugDraw::RaylibDebugDraw(std::unique_ptr<Resources> resources) noexcept
    : resources_(std::move(resources)) {}

RaylibDebugDraw::RaylibDebugDraw(RaylibDebugDraw&& other) noexcept = default;

RaylibDebugDraw& RaylibDebugDraw::operator=(RaylibDebugDraw&& other) noexcept = default;

RaylibDebugDraw::~RaylibDebugDraw() = default;

void RaylibDebugDraw::assert_not_moved_from() const {
    APLOMB_ASSERT(resources_ != nullptr, "a backend is used only until it is moved from");
}

RaylibDebugDraw::Resources& RaylibDebugDraw::resources() const {
    assert_not_moved_from();
    return *resources_;
}

bool RaylibDebugDraw::closing() const {
    assert_not_moved_from();
    return WindowShouldClose();
}

float RaylibDebugDraw::frame_seconds() const {
    assert_not_moved_from();
    return GetFrameTime();
}

void RaylibDebugDraw::begin_frame() {
    assert_not_moved_from();
    BeginDrawing();
    ClearBackground(background);
    rlImGuiBegin();
}

void RaylibDebugDraw::begin_scene(const CameraView& view) {
    assert_not_moved_from();
    BeginMode3D({
        .position = to_raylib(view.eye),
        .target = to_raylib(view.target),
        .up = {.x = 0.0F, .y = 1.0F, .z = 0.0F},
        .fovy = JPH::RadiansToDegrees(view.vertical_fov),
        .projection = CAMERA_PERSPECTIVE,
    });
}

void RaylibDebugDraw::end_scene() {
    resources().draw_all_lines();
    EndMode3D();
}

void RaylibDebugDraw::end_frame() {
    assert_not_moved_from();
    rlImGuiEnd();
    EndDrawing();
}

void RaylibDebugDraw::line(JPH::Vec3 from, JPH::Vec3 to, JPH::Color color) {
    Resources& drawn = resources();
    drawn.line_vertices.push_back({.position = from, .color = color});
    drawn.line_vertices.push_back({.position = to, .color = color});
}

void RaylibDebugDraw::overlay_line(JPH::Vec3 from, JPH::Vec3 to, JPH::Color color) {
    Resources& drawn = resources();
    drawn.overlay_vertices.push_back({.position = from, .color = color});
    drawn.overlay_vertices.push_back({.position = to, .color = color});
}

void RaylibDebugDraw::box(const aplomb::Transform& transform, JPH::Vec3 half_extents,
                          JPH::Color color) {
    const Resources& drawn = resources();
    draw_mesh(drawn.cube, drawn.material, placement(transform) * JPH::Mat44::sScale(half_extents),
              color);
}

void RaylibDebugDraw::capsule(const aplomb::Transform& transform, float radius, float half_height,
                              JPH::Color color) {
    const Resources& drawn = resources();
    const JPH::Mat44 place = placement(transform);
    const JPH::Mat44 cap = JPH::Mat44::sScale(radius);
    draw_mesh(drawn.cylinder, drawn.material,
              place * JPH::Mat44::sTranslation(JPH::Vec3(0.0F, -half_height, 0.0F)) *
                  JPH::Mat44::sScale(JPH::Vec3(radius, 2.0F * half_height, radius)),
              color);
    draw_mesh(drawn.sphere, drawn.material,
              place * JPH::Mat44::sTranslation(JPH::Vec3(0.0F, half_height, 0.0F)) * cap, color);
    draw_mesh(drawn.sphere, drawn.material,
              place * JPH::Mat44::sTranslation(JPH::Vec3(0.0F, -half_height, 0.0F)) * cap, color);
}

void RaylibDebugDraw::sphere(const aplomb::Transform& transform, float radius, JPH::Color color) {
    const Resources& drawn = resources();
    draw_mesh(drawn.sphere, drawn.material, placement(transform) * JPH::Mat44::sScale(radius),
              color);
}
