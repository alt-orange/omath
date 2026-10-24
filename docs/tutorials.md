# Tutorials

This page provides step-by-step tutorials for common OMath use cases.

!!! note "Namespaces used below"
    Engine types live in their own namespaces (`omath::source_engine`, `omath::unity_engine`, ...) and each defines its own `PitchAngle`, `YawAngle`, `RollAngle`, `ViewAngles`, `Mat4X4` and `Camera`. The tutorials use short namespace aliases (`se`, `proj`) instead of `using namespace`, because `using namespace omath;` together with `using namespace omath::source_engine;` makes `ViewAngles` ambiguous.

---

## Tutorial 1: Basic Vector Math

Learn the fundamentals of vector operations in OMath.

### Step 1: Include OMath

```cpp
#include <omath/omath.hpp>
#include <iostream>

using omath::Vector2;
using omath::Vector3;
using omath::Vector4;
```

### Step 2: Create Vectors

```cpp
// 2D vectors
Vector2<float> v2a{3.0f, 4.0f};
Vector2<float> v2b{1.0f, 2.0f};

// 3D vectors
Vector3<float> v3a{1.0f, 2.0f, 3.0f};
Vector3<float> v3b{4.0f, 5.0f, 6.0f};

// 4D vectors (often used for homogeneous coordinates)
Vector4<float> v4{1.0f, 2.0f, 3.0f, 1.0f};
```

### Step 3: Perform Operations

```cpp
// Addition
auto sum = v3a + v3b;  // {5, 7, 9}

// Subtraction
auto diff = v3a - v3b;  // {-3, -3, -3}

// Scalar multiplication
auto scaled = v3a * 2.0f;  // {2, 4, 6}

// Dot product
float dot = v3a.dot(v3b);  // 32.0

// Cross product (3D only)
auto cross = v3a.cross(v3b);  // {-3, 6, -3}

// Length
float len = v3a.length();  // ~3.74

// Normalization (safe - returns the vector unchanged if its length is zero)
auto normalized = v3a.normalized();

// Distance between vectors
float dist = v3a.distance_to(v3b);  // ~5.196
```

### Step 4: Angle Calculations

```cpp
if (auto angle = v3a.angle_between(v3b)) {
    std::cout << "Angle in degrees: " << angle->as_degrees() << "\n";
    std::cout << "Angle in radians: " << angle->as_radians() << "\n";
} else {
    std::cout << "Cannot compute angle (zero-length vector)\n";
}

// Check if perpendicular
if (v3a.is_perpendicular(v3b)) {
    std::cout << "Vectors are perpendicular\n";
}
```

### Step 5: Format and Hash

```cpp
#include <format>

std::cout << std::format("{}", v3a) << "\n";  // [1, 2, 3]

std::unordered_map<Vector3<float>, int> visited;  // std::hash<Vector3<T>> is provided
```

**Key takeaways:**
- All vector operations are type-safe and constexpr-friendly
- Safe normalization never produces NaN
- Angle calculations use `std::expected` for error handling, `length()`/`normalized()`/`distance_to()` need a floating point type

---

## Tutorial 2: World-to-Screen Projection

Project 3D coordinates to 2D screen space for overlays and ESP.

### Step 1: Choose Your Game Engine

```cpp
#include <omath/omath.hpp>

// For Source Engine games (CS:GO, TF2, etc.)
namespace se = omath::source_engine;

// Or for other engines:
// namespace eng = omath::unity_engine;
// namespace eng = omath::unreal_engine;
// namespace eng = omath::frostbite_engine;
// namespace eng = omath::cry_engine;
// namespace eng = omath::rage_engine;
```

### Step 2: Set Up the Camera

```cpp
namespace proj = omath::projection;

// Define viewport (screen dimensions)
proj::ViewPort viewport{1920.0f, 1080.0f};

// Define field of view
auto fov = proj::FieldOfView::from_degrees(90.0f);

// Camera position and angles
omath::Vector3<float> camera_pos{0.0f, 0.0f, 100.0f};
se::ViewAngles camera_angles{
    se::PitchAngle::from_degrees(0.0f),
    se::YawAngle::from_degrees(0.0f),
    se::RollAngle::from_degrees(0.0f)
};

// Create camera (using Source Engine in this example)
se::Camera camera(
    camera_pos,
    camera_angles,
    viewport,
    fov,
    0.1f,    // near plane
    1000.0f  // far plane
);
```

!!! note
    Unreal's camera is double precision: pass `omath::Vector3<double>` positions and `unreal_engine::ViewAngles`. All other engines use `float`.

### Step 3: Project 3D Points

```cpp
// 3D world position (e.g., enemy player position)
omath::Vector3<float> enemy_pos{150.0f, 200.0f, 75.0f};

// Project to screen
if (auto screen = camera.world_to_screen(enemy_pos)) {
    std::cout << "Enemy on screen at: "
              << screen->x << ", " << screen->y << "\n";

    // Draw ESP box or marker at screen->x, screen->y
    // Note: screen coordinates are in viewport space (0-width, 0-height),
    // origin in the top-left corner. screen->z is the normalized depth.
} else {
    // Enemy is not visible
    switch (screen.error()) {
        case proj::Error::WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS:
            std::cout << "Outside the screen\n";
            break;
        case proj::Error::PERSPECTIVE_DIVIDER_LESS_EQ_ZERO:
            std::cout << "Behind the camera\n";
            break;
        case proj::Error::INV_VIEW_PROJ_MAT_DET_EQ_ZERO:
            break; // only produced by screen_to_world()
    }
}
```

`world_to_screen()` rejects points outside the screen. If you want to draw a box that is partly off-screen, use `world_to_screen_unclipped()`, which still rejects points behind the camera but keeps points outside the viewport.

### Step 4: Update Camera for Each Frame

The camera caches its view and projection matrices and rebuilds them lazily when a setter changes an input:

```cpp
void render_frame() {
    // Read current camera data from game
    omath::Vector3<float> new_pos = read_camera_position();
    se::ViewAngles new_angles = read_camera_angles();

    // Update camera
    camera.set_origin(new_pos);
    camera.set_view_angles(new_angles);

    // Project all entities
    for (const auto& entity : entities) {
        if (auto screen = camera.world_to_screen(entity.position)) {
            draw_esp_box(screen->x, screen->y);
        }
    }
}
```

If the game gives you its own view-projection matrices, `Camera::calc_view_angles_from_view_matrix()`, `calc_origin_from_view_matrix()` and `extract_projection_params()` recover the camera inputs from them.

### Step 5: A Radar

`world_to_radar()` turns a world position into a 2D offset from the centre of a radar, rotated by the camera yaw:

```cpp
#include <omath/algorithm/radar.hpp>

// scale: radar pixels per world unit
const omath::Vector2<float> blip = omath::algorithm::world_to_radar(camera, enemy_pos, 0.1f);
draw_radar_blip(radar_center.x + blip.x, radar_center.y + blip.y);
```

See [Radar](algorithm/radar.md) and [Targeting](algorithm/targeting.md) for the closest-target helpers.

**Key takeaways:**
- Choose the engine trait that matches your target game
- `world_to_screen()` returns `std::expected<Vector3, projection::Error>` - always check the result
- Update the camera each frame with `set_origin()` / `set_view_angles()` for accurate projections
- Screen coordinates are in the viewport space you defined, origin top-left by default

---

## Tutorial 3: Projectile Prediction (Aim-Bot)

Calculate where to aim to hit a moving target.

### Step 1: Define Projectile and Launcher

The round and the weapon are described separately: `Projectile` holds the physical properties, `Launcher` holds where the round leaves from.

```cpp
#include <omath/omath.hpp>

using namespace omath::projectile_prediction;

// The round itself
constexpr Projectile<float> bullet{
    .m_launch_speed = 800.0f,   // muzzle velocity (game units/s)
    .m_gravity_scale = 1.0f,    // multiplier for the engine's gravity constant
};

// Who fires, and from where
constexpr Launcher<float> launcher{
    .eye_origin = {0, 0, 64},                                      // the eye the view angles apply to
    .muzzle_offset = {.forward = 16.f, .right = 8.f, .up = -6.f},  // spawn point, rotates with the view
};
```

### Step 2: Define Target State

```cpp
// Target information (enemy player)
constexpr Target<float> enemy{
    .m_origin = {100, 200, 50},   // current position
    .m_velocity = {10, 5, 0},     // current velocity
    .m_is_airborne = false,       // airborne targets also fall under gravity
};
```

### Step 3: Calculate the Aim

```cpp
// Create the prediction engine (Source-style units: gravity 800 units/s^2)
const ProjPredEngineLegacy<> engine(/*gravity*/ 800.f,
                                    /*time step*/ 1.f / 1000.f,
                                    /*max flight time*/ 10.f,
                                    /*distance tolerance*/ 5.f);

if (const auto aim = engine.maybe_calculate_aim(bullet, launcher, enemy)) {
    std::cout << "Pitch: " << aim->angles.pitch << ", yaw: " << aim->angles.yaw << "\n";
    std::cout << "Time of flight: " << aim->time_of_flight << " s\n";
    std::cout << "Aim at: " << aim->aim_point.x << ", " << aim->aim_point.y << ", " << aim->aim_point.z << "\n";

    // aim->predicted_target_position is where the target will be when the round arrives.
    // aim->aim_point lies on the aim ray - project it with Camera::world_to_screen() to draw a marker.
} else {
    // Cannot hit target (too fast, out of range, ...)
    std::cout << "Target cannot be hit\n";
}
```

`AimAngles` are in degrees and pitch is **positive upwards** for every engine. Source's own `PitchAngle` is positive downwards, so with a Source camera apply it as `se::PitchAngle::from_degrees(-aim->angles.pitch)`.

The engine type defaults to `source_engine::PredEngineTrait`. For another engine pass its trait: `ProjPredEngineLegacy<omath::unity_engine::PredEngineTrait>`.

### Step 4: Handle Different Scenarios

```cpp
// Stationary target: the engine still compensates for gravity drop (the solved pitch is above the direct line)
constexpr Target<float> stationary{.m_origin = {100, 100, 100}, .m_velocity = {0, 0, 0}};

// Target moving faster than the round can fly
constexpr Target<float> fast{.m_origin = {100, 100, 100}, .m_velocity = {1500, 0, 0}};
// Returns nullopt: the round (800 units/s) cannot catch up

// Falling target: m_is_airborne makes the engine apply gravity to the target too
constexpr Target<float> aerial{.m_origin = {100, 100, 200}, .m_velocity = {5, 5, -10}, .m_is_airborne = true};

// A steep lob instead of the flat shot (grenades, mortars)
const ProjPredEngineLegacy<> lob_engine(800.f, 1.f / 1000.f, 10.f, 5.f, Arc::HIGH);

// Weapons that launch above the crosshair (TF2 pipes: 9.46 degrees)
constexpr Launcher<float> pipe_launcher{.eye_origin = {0, 0, 64}, .launch_pitch_offset = 9.46f};
```

### Step 5: Other Engines

```cpp
// Vectorised scan, float only, Source-style axes (z up).
// Requires a library built with OMATH_USE_AVX2 on x86: every call throws otherwise.
const ProjPredEngineAvx2 fast_engine(800.f, 1.f / 1000.f, 10.f);
if (const auto aim = fast_engine.maybe_calculate_aim(bullet, launcher, enemy)) {
    // Same AimSolution as the legacy engine
}

// Rounds with air drag (set Projectile::m_drag, m_drag_forward, m_drag_up, m_max_speed):
// the step must match the game's physics tick, 0.015 s for Team Fortress 2.
const ProjPredEngineDrag<> drag_engine(800.f, 0.015f, 10.f, 5.f);
```

All engines implement `ProjPredEngineInterface`, so they can be swapped without touching the code that consumes the `AimSolution`. The old `maybe_calculate_aim_point(projectile, target)` / `maybe_calculate_aim_angles(projectile, target)` wrappers still exist for code written before `Launcher` was introduced; new code should call `maybe_calculate_aim()`.

**Key takeaways:**
- Always check if an aim solution exists before using it
- Velocity, speed and gravity must use the same units as positions
- The gravity constant is a magnitude (800 for Source); the engine applies it along the engine's own down axis
- `ProjPredEngineAvx2` only works in AVX2 builds, `ProjPredEngineLegacy` works everywhere
- Returns `nullopt` when the target is unreachable

---

## Tutorial 4: Collision Detection

Perform ray-casting and intersection tests.

### Step 1: Ray vs Triangle

`LineTracer` is a static helper. A `Ray` is a segment from `start` to `end`; set `infinite_length` to keep going past `end`. The functions return the hit point, or `ray.end` when nothing was hit:

```cpp
#include <omath/omath.hpp>

using omath::Vector3;
using Tracer = omath::collision::LineTracer<>;

// Ray from above the origin, pointing down
const omath::collision::Ray<> ray{.start = {10, 20, 100}, .end = {10, 20, -100}};

const omath::Triangle<Vector3<float>> ground{{-100, -100, 0}, {100, -100, 0}, {0, 100, 0}};

const auto hit = Tracer::get_ray_hit_point(ray, ground);
if (hit != ray.end) {
    std::cout << "Hit ground at: " << hit.x << ", " << hit.y << ", " << hit.z << "\n";
    // Expected: (10, 20, 0)
} else {
    std::cout << "Ray does not intersect the triangle\n";
}

// Visibility check: true only when nothing blocks the whole segment
const bool clear = Tracer::can_trace_line(ray, ground);
```

### Step 2: Axis-Aligned and Oriented Boxes

```cpp
#include <omath/3d_primitives/aabb.hpp>
#include <omath/3d_primitives/obb.hpp>

// Create a bounding box
omath::primitives::Aabb<float> bbox{
    Vector3<float>{0, 0, 0},       // min corner
    Vector3<float>{100, 100, 100}  // max corner
};

const auto center = bbox.center();     // (50, 50, 50)
const auto extents = bbox.extents();   // half size: (50, 50, 50)

// Box-box intersection
omath::primitives::Aabb<float> other{
    Vector3<float>{50, 50, 50},
    Vector3<float>{150, 150, 150}
};

if (bbox.is_collide(other)) {
    std::cout << "Boxes overlap\n";
}

// Ray vs box
const omath::collision::Ray<> down{.start = {50, 50, 200}, .end = {50, 50, -200}};
const auto box_hit = Tracer::get_ray_hit_point(down, bbox);
if (box_hit != down.end) {
    std::cout << "Entry point: " << box_hit.z << "\n";  // 100
}

// An oriented box: center, three orthonormal axes and the half size along each
omath::primitives::Obb<float> obb{
    .center = {0, 0, 0},
    .axis_x = {1, 0, 0}, .axis_y = {0, 1, 0}, .axis_z = {0, 0, 1},
    .half_extents = {1, 2, 3},
};
const auto corners = obb.vertices();  // std::array<Vector3<float>, 8>
```

`Aabb` takes an optional second template parameter, the up axis (`UpAxis::Y` by default), which `top()` and `bottom()` use. Use `UpAxis::Z` for Z-up engines.

### Step 3: Convex Shapes with GJK and EPA

Wrap a mesh in an engine's `MeshCollider` and ask GJK whether two shapes overlap; EPA then gives the penetration depth and direction:

```cpp
#include <omath/collision/epa_algorithm.hpp>
#include <omath/collision/gjk_algorithm.hpp>
#include <omath/engines/source_engine/collider.hpp>

namespace se = omath::source_engine;

// A unit cube: eight corners, no faces needed for the support function
const se::Mesh cube_mesh = {
    {
        {{-1.f, -1.f, -1.f}, {}, {}}, {{-1.f, -1.f, 1.f}, {}, {}},
        {{-1.f, 1.f, -1.f}, {}, {}},  {{-1.f, 1.f, 1.f}, {}, {}},
        {{1.f, 1.f, 1.f}, {}, {}},    {{1.f, 1.f, -1.f}, {}, {}},
        {{1.f, -1.f, 1.f}, {}, {}},   {{1.f, -1.f, -1.f}, {}, {}},
    },
    {}
};

auto moved = cube_mesh;
moved.set_origin({0.f, 0.5f, 0.f});

const se::MeshCollider collider_a(cube_mesh);
const se::MeshCollider collider_b(moved);

using Gjk = omath::collision::GjkAlgorithm<se::MeshCollider>;
using Epa = omath::collision::Epa<se::MeshCollider>;

if (const auto info = Gjk::is_collide_with_simplex_info(collider_a, collider_b); info.hit) {
    if (const auto penetration = Epa::solve(collider_a, collider_b, info.simplex)) {
        std::cout << "Depth: " << penetration->depth << "\n";            // 1.5
        std::cout << "Normal y: " << penetration->normal.y << "\n";
    }
}
```

`Gjk::is_collide()` is the shortcut when you only need the yes/no answer. To collide your own shapes implement [`ColliderInterface`](collision/collider_interface.md).

### Step 4: Frustum Culling

Cameras can cull boxes before you project them:

```cpp
omath::primitives::Aabb<float> entity_box{{-10, -10, 0}, {10, 10, 70}};

if (!camera.is_aabb_culled_by_frustum(entity_box)) {
    // possibly visible - project and draw
}
```

There is also `is_obb_culled_by_frustum()` and `is_culled_by_frustum()` for a triangle.

**Key takeaways:**
- A ray is a segment: the hit point equals `ray.end` when nothing was hit
- `Aabb` uses `is_collide()`, there are no `contains()`/`intersects()` members
- `LineTracer` traces against triangles, `Aabb`, `Obb` and meshes
- GJK answers "do they overlap", EPA answers "by how much"
- AABB tests are very fast for broad-phase collision detection

---

## Tutorial 5: Pattern Scanning

Search for byte patterns in memory and in executable files.

### Step 1: Basic Pattern Scanning

```cpp
#include <omath/utility/pattern_scan.hpp>
#include <vector>

// Memory to search (e.g., from a loaded module)
std::vector<std::byte> memory = /* ... */;
std::span<std::byte> view{memory};

// Pattern with wildcards (?? or ? = match any byte)
const auto found = omath::PatternScanner::scan_for_pattern(view, "48 8B 05 ?? ?? ?? ?? 48 85 C0");

if (found != view.end()) {
    std::cout << "Pattern found at offset: " << (found - view.begin()) << "\n";
} else {
    std::cout << "Pattern not found\n";
}
```

The result is an iterator into the range you passed in; `end()` means "not found" (an invalid pattern string also returns `end()`). An rvalue span is rejected at compile time, so scan a named `std::span`.

### Step 2: Compile-Time Patterns and Iterator Ranges

```cpp
// The pattern is parsed at compile time; a malformed one fails to compile
const auto it = omath::PatternScanner::scan_for_pattern<"E8 ?? ?? ?? ?? 85 C0">(memory.begin(), memory.end());

// Runtime pattern over any iterator pair
const auto it2 = omath::PatternScanner::scan_for_pattern(memory.begin(), memory.end(), "E8 ? ? ? ?");
```

### Step 3: Scan a PE File

```cpp
#include <omath/utility/pe_pattern_scan.hpp>

const auto result = omath::PePatternScanner::scan_for_pattern_in_file("game.exe", "E8 ?? ?? ?? ?? 85 C0 75 ??", ".text");

if (result) {
    // virtual_base_addr: where the section is mapped (image base + section RVA) in the file's headers
    // raw_base_addr: offset of the section inside the file; target_offset: match offset inside the section
    std::cout << "Virtual address: 0x" << std::hex << (result->virtual_base_addr + result->target_offset)
              << ", file offset: 0x" << (result->raw_base_addr + result->target_offset) << std::dec << "\n";
} else {
    std::cout << "Pattern not found in PE file\n";
}
```

`.text` is the default section; pass another name to scan a different section. The addresses describe the file on disk (its preferred image base); to get an address in a running process scan the loaded module instead (next step). The compile-time form is `scan_for_pattern_in_file<"E8 ?? ?? ?? ??">("game.exe")`.

### Step 4: Scan a Loaded Module

Inside the target process (an injected DLL) scan the module that is already mapped, the result is an absolute address:

```cpp
const void* module_base = /* GetModuleHandleA("game.exe") */ nullptr;

if (const auto address = omath::PePatternScanner::scan_for_pattern_in_loaded_module(module_base, "48 8B 05 ?? ?? ?? ??")) {
    std::cout << "Found at 0x" << std::hex << *address << std::dec << "\n";
}
```

### Step 5: ELF and Mach-O

`ElfPatternScanner` (`<omath/utility/elf_pattern_scan.hpp>`, default section `.text`) and `MachOPatternScanner` (`<omath/utility/macho_pattern_scan.hpp>`, default section `__text`) have the same three entry points as the PE scanner: `scan_for_pattern_in_file`, `scan_for_pattern_in_loaded_module` and `scan_for_pattern_in_memory_file` (a file image already loaded into a `std::span<const std::byte>`):

```cpp
#include <omath/utility/elf_pattern_scan.hpp>

const auto in_so = omath::ElfPatternScanner::scan_for_pattern_in_file("libgame.so", "E8 ?? ?? ?? ??");
```

**Key takeaways:**
- Use `??` (or `?`) in pattern strings for wildcards; every byte is exactly two hex digits separated by spaces
- The PE, ELF and Mach-O scanners work on files, memory images and loaded modules
- Pattern scanning is useful for finding functions, vtables, or data
- Always validate found addresses before use
- Patterns may have multiple matches - consider context

---

## Tutorial 6: Angles and View Angles

Work with game camera angles properly.

### Step 1: Understanding Angle Types

```cpp
#include <omath/omath.hpp>

namespace se = omath::source_engine;

// Generic angle with custom range (wraps around by default)
auto angle1 = omath::Angle<float, 0.0f, 360.0f>::from_degrees(45.0f);
auto angle2 = omath::Angle<float, -180.0f, 180.0f>::from_degrees(270.0f);  // -90

// Clamped instead of wrapped
using Clamped = omath::Angle<float, -90.0f, 90.0f, omath::AngleFlags::Clamped>;
auto limited = Clamped::from_degrees(120.0f);                               // 90

// Specialized camera angles of one engine (Source: pitch clamped to +-89)
auto pitch = se::PitchAngle::from_degrees(-10.0f);
auto yaw = se::YawAngle::from_degrees(90.0f);
auto roll = se::RollAngle::from_degrees(0.0f);
```

### Step 2: Angle Conversions

```cpp
// Create from degrees
auto deg_angle = se::PitchAngle::from_degrees(45.0f);

// Get as radians
float radians = deg_angle.as_radians();
std::cout << "45 degrees = " << radians << " radians\n";

// Get as degrees
float degrees = deg_angle.as_degrees();
std::cout << "Value: " << degrees << " degrees\n";

// Create from radians or from inverse trigonometric functions
auto from_rad = se::YawAngle::from_radians(1.5707963f);
auto from_atan = se::YawAngle::from_atan2(1.0f, 1.0f);  // 45
```

### Step 3: View Angles (Camera)

```cpp
// Pitch: vertical rotation, Yaw: horizontal rotation, Roll: camera tilt
// Source Engine ranges: pitch [-89, 89], yaw [-180, 180], roll [-180, 180]

se::ViewAngles camera_angles{
    se::PitchAngle::from_degrees(-15.0f),
    se::YawAngle::from_degrees(45.0f),
    se::RollAngle::from_degrees(0.0f)
};

// Access individual components
float pitch_val = camera_angles.pitch.as_degrees();
float yaw_val = camera_angles.yaw.as_degrees();
float roll_val = camera_angles.roll.as_degrees();

// Or all three as a Vector3 (x = pitch, y = yaw, z = roll)
omath::Vector3<float> as_vector = camera_angles.as_vector3();
```

### Step 4: Calculating Look-At Angles

```cpp
omath::Vector3<float> camera_pos{0, 0, 100};
omath::Vector3<float> target_pos{100, 100, 100};

// Calculate angles to look at target through the engine's camera trait...
se::ViewAngles look_at = se::CameraTrait::calc_look_at_angle(camera_pos, target_pos);

// ...or through a camera, which uses its own origin
se::ViewAngles from_camera = camera.calc_look_at_angles(target_pos);

std::cout << "Pitch: " << look_at.pitch.as_degrees() << "\n";
std::cout << "Yaw: " << look_at.yaw.as_degrees() << "\n";
std::cout << "Roll: " << look_at.roll.as_degrees() << "\n";
```

### Step 5: Angle Arithmetic

```cpp
// Angles support arithmetic with automatic normalization
auto angle_a = se::YawAngle::from_degrees(170.0f);
auto angle_b = se::YawAngle::from_degrees(20.0f);

// Addition (wraps around)
auto sum = angle_a + angle_b;  // 190 degrees -> normalized to -170

// Subtraction
auto diff = angle_b - angle_a;  // -150

// Negation and comparison
auto negated = -angle_a;
bool less = angle_b < angle_a;
```

There is no scalar multiplication; compute on `as_degrees()` and build a new angle with `from_degrees()` when you need to scale.

**Key takeaways:**
- Use the engine's angle types for camera angles (`PitchAngle`, `YawAngle`, `RollAngle`)
- Angles automatically normalize or clamp to their valid ranges
- Each game engine has different angle ranges and axis conventions
- Use the camera trait or `Camera::calc_look_at_angles()` to get look-at angles in the engine's convention

---

## Tutorial 7: Typed Access to Game Memory

Read game structures with named accessors instead of raw offsets.

### Step 1: Write a Memory Trait

An external object reads memory through a trait you write. It needs a templated `read_memory<T>(address)` and, for writing, `write_memory(address, value)`:

```cpp
#include <omath/rev_eng/external_rev_object.hpp>

struct SelfTrait {
    template<class T>
    static T read_memory(std::uintptr_t address) { return *reinterpret_cast<const T*>(address); }

    template<class T>
    static void write_memory(std::uintptr_t address, const T& value) { *reinterpret_cast<T*>(address) = value; }
};
```

In a real tool the bodies call `ReadProcessMemory`, `process_vm_readv`, a driver or a DMA device. See [Memory trait](rev_eng/memory_trait.md).

### Step 2: Describe the Structure

```cpp
class Player final : public omath::rev_eng::ExternalReverseEngineeredObject<SelfTrait> {
public:
    using ExternalReverseEngineeredObject::ExternalReverseEngineeredObject;

    [[nodiscard]] omath::Vector3<float> origin() const { return get_by_offset<omath::Vector3<float>>(0x134); }
    [[nodiscard]] int health() const { return get_by_offset<int>(0x140); }
    void set_health(int value) const { set_by_offset(0x140, value); }
};

Player local_player{local_player_address};
auto pos = local_player.origin();
```

`get_by_offset` and `set_by_offset` are protected, so the structure is always a subclass. When your code runs inside the target use `InternalReverseEngineeredObject` instead: it returns references straight into the object and can call game functions by pattern or by vtable index (`call_method`, `call_virtual_method`). See [External object](rev_eng/external_rev_object.md) and [Internal object](rev_eng/internal_rev_object.md).

### Step 3: Find the Addresses with Signatures

Offsets and function addresses change between game builds. Resolve them with a signature scan (Tutorial 5) at start-up instead of hard-coding them.

---

## Tutorial 8: Drawing an ESP Overlay

The HUD toolkit describes what to draw as small widget structs; a renderer backend turns them into draw calls. It needs a build with `OMATH_IMGUI_INTEGRATION` for the ready-made ImGui backend.

### Step 1: Screen-Space Widgets

```cpp
#include <omath/hud/renderer_realizations/imgui_renderer.hpp>
#include <omath/hud/screen_overlay.hpp>

const auto renderer = std::make_shared<omath::hud::ImguiHudRenderer>();

omath::hud::ScreenOverlay overlay{{1920.f, 1080.f}, renderer};
overlay.add_crosshair({.color = omath::Color::green()});
overlay.add_fov_circle({.color = omath::Color::red(), .radius = 120.f});
```

Call this between `ImGui::NewFrame()` and `ImGui::Render()`, the renderer draws into ImGui's background draw list.

### Step 2: Per-Entity Widgets

`EntityOverlay::from_aabb` projects an entity's bounding box with your camera and returns an overlay positioned on it (or an `EntityOverlayError` when nothing could be projected):

```cpp
#include <omath/hud/entity_overlay.hpp>

omath::primitives::Aabb<float> enemy_box{{10, -10, -10}, {30, 10, 10}};

if (auto overlay = omath::hud::EntityOverlay::from_aabb(camera, enemy_box, /*aspect*/ 4.f, renderer)) {
    overlay->contents(omath::hud::widget::Box{omath::Color::green()});
}
```

`contents(...)` accepts any number of widgets (`Box`, `CorneredBox`, `Bar`, `Label`, `Skeleton`, ...) and `when(condition, widget)` makes one conditional. To draw with something other than ImGui, implement [`HudRendererInterface`](hud/renderer_interface.md). See [HUD overview](hud/overview.md), [Widgets](hud/widgets.md), [Screen overlay](hud/screen_overlay.md) and [Entity overlay](hud/entity_overlay.md).

---

## Tutorial 9: Hooking a Game's Render Loop

To draw inside another process you need a callback that runs every frame. `HooksManager` finds and hooks the present call of DirectX 9/11/12 or OpenGL for you. It needs a build with `OMATH_ENABLE_HOOKING`; without it the class has no hook methods.

```cpp
#include <omath/hooks/hooks_manager.hpp>

auto& hooks = omath::hooks::HooksManager::get();

hooks.set_on_present([](IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    // create your ImGui context on the first call, then draw the overlay every frame
});
hooks.set_on_resize_buffers([](IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) {
    // release render targets here
});

if (!hooks.hook_dx11()) {
    // hooking failed
}

// on unload
hooks.unhook_dx11();
```

Use `hook_dx9` / `hook_dx12` / `hook_opengl` for the other APIs, and `hook_wnd_proc` to intercept window messages for input. The repository's `examples/example_dx9_hook`, `example_dx11_hook`, `example_dx12_hook` and `example_opengl_hook` are complete injectable DLLs. See [HooksManager](hooks/hooks_manager.md).

---

## Tutorial 10: Scripting with Lua

With the `OMATH_ENABLE_LUA` build option you can expose OMath to a Lua state:

```cpp
#include <lua.hpp>
#include <omath/lua/lua.hpp>

lua_State* L = luaL_newstate();
luaL_openlibs(L);
omath::lua::LuaInterpreter::register_lib(L);   // creates the global `omath` table

luaL_dostring(L, R"(
    local a = omath.Vec3.new(1, 2, 3)
    local b = omath.Vec3.new(4, 5, 6)
    print(a:dot(b))
)");

lua_close(L);
```

The `omath` table contains vectors, matrices, quaternions, colors, collision, pattern scanning, the HUD and one sub-table per engine (`omath.source`, `omath.unity`, `omath.unreal`, `omath.frostbite`, `omath.iw`, `omath.opengl`, `omath.cry`, `omath.rage`, `omath.serious`). Fallible calls such as `camera:world_to_screen(pos)` return `nil, error_message` instead of an `std::expected`. See [Lua scripting](lua/lua.md).

---

## Next Steps

Now that you've completed these tutorials, explore:

- **[API Overview](api_overview.md)** - Map of every module
- **[Engine Documentation](engines/source_engine/camera_trait.md)** - Engine-specific features
- **[Examples](../examples/)** - More code examples
- **[Getting Started](getting_started.md)** - Quick start guide
