# Getting Started
Welcome to OMath! This guide will help you get up and running with the library quickly.

## What is OMath?

OMath is a modern, blazingly fast C++23 math library designed for:
- **Game development** and cheat development
- **Graphics programming** (DirectX/OpenGL/Vulkan)
- **3D applications** with support for multiple game engines
- **High-performance computing** with AVX2 optimizations

Key features:
- 100% independent, no legacy C++ code
- Fully `constexpr` template-based design
- Zero additional dependencies for the core library (ImGui, Lua and safetyhook are needed only for the optional features, GoogleTest only for the unit tests)
- Cross-platform (Windows, macOS, Linux)
- Built-in support for Source, Unity, Unreal, Frostbite, IW Engine, CryEngine, RAGE, Serious Engine and OpenGL coordinate systems
- Projectile prediction, collision detection (GJK/EPA, ray tracing), pathfinding and a walk bot
- A HUD toolkit for ESP-style overlays, DirectX/OpenGL hooking and Lua bindings (all opt-in)
- Reverse engineering helpers: typed memory access and byte pattern scanning for PE, ELF and Mach-O

---

## Installation

Choose one of the following methods to install OMath:

### Using vcpkg (Recommended)

```bash
vcpkg install orange-math
```

Then in your CMakeLists.txt:
```cmake
find_package(omath CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE omath::omath)
```

### Using xrepo

```bash
xrepo install omath
```

Then in your xmake.lua:
```lua
add_requires("omath")
target("your_target")
    add_packages("omath")
```

### Using Conan

```bash
conan install --requires="omath/[*]" --build=missing
```

### Building from Source

See the detailed [Installation Guide](install.md) for complete instructions, including the CMake options and vcpkg features (`imgui`, `lua`, `hooking`, `avx2`) that switch the optional parts on.

!!! note
    OMath needs a C++23 compiler. `<omath/omath.hpp>` pulls in most of the library, but a few headers are not part of it (radar, targeting, the engine mesh/collider/primitive headers, the HUD, the hooks, the Lua bindings and the ELF/Mach-O scanners). Include those explicitly.

---

## Quick Example

Here's a simple example to get you started:

```cpp
#include <omath/omath.hpp>
#include <iostream>

int main() {
    using omath::Vector3;

    // Create 3D vectors
    Vector3<float> a{1.0f, 2.0f, 3.0f};
    Vector3<float> b{4.0f, 5.0f, 6.0f};

    // Vector operations
    auto sum = a + b;                    // Vector addition
    auto dot_product = a.dot(b);         // Dot product: 32.0
    auto cross_product = a.cross(b);     // Cross product: (-3, 6, -3)
    auto length = a.length();            // Length: ~3.74
    auto normalized = a.normalized();    // Unit vector

    std::cout << "Sum: [" << sum.x << ", " << sum.y << ", " << sum.z << "]\n";
    std::cout << "Dot product: " << dot_product << "\n";
    std::cout << "Length: " << length << "\n";

    return 0;
}
```

---

## Core Concepts

### 1. Vectors

OMath provides 2D, 3D, and 4D vector types. `Vector3` derives from `Vector2` and `Vector4` from `Vector3`:

```cpp
using omath::Vector2;
using omath::Vector3;
using omath::Vector4;

Vector2<float> vec2{1.0f, 2.0f};
Vector3<float> vec3{1.0f, 2.0f, 3.0f};
Vector4<float> vec4{1.0f, 2.0f, 3.0f, 4.0f};
```

All vector types support:
- Arithmetic operations (+, -, *, /)
- Dot products (`Vector3` also has `cross`)
- Length and distance calculations (floating point types)
- Normalization (`normalized()` returns the vector unchanged when its length is zero)
- `std::hash` and `std::format` support

See: [Vector2](linear_algebra/vector2.md), [Vector3](linear_algebra/vector3.md), [Vector4](linear_algebra/vector4.md), and [Quaternion](linear_algebra/quaternion.md) for rotations.

### 2. Matrices

`Mat<Rows, Columns, Type, StoreType>` is the generic matrix. Every engine has a `Mat4X4` alias for its own storage order, and helper functions build the usual transforms:

```cpp
using Mat4 = omath::Mat<4, 4, float, omath::MatStoreType::ROW_MAJOR>;

Mat4 identity{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
auto translation = omath::mat_translation(omath::Vector3<float>{1.f, 2.f, 3.f});
auto combined = translation * identity;

if (const auto inverse = combined.inverted()) {
    // inverted() returns std::optional - empty for a singular matrix
}
```

See: [Matrix Documentation](linear_algebra/mat.md)

### 3. Angles

Strong-typed angle system with automatic range management. A generic `Angle<Type, Min, Max, Flags>` is wrapped (normalized or clamped) into its range, and each engine defines its own `PitchAngle`, `YawAngle`, `RollAngle` and `ViewAngles`:

```cpp
namespace se = omath::source_engine;

auto angle = omath::Angle<float, 0.0f, 360.0f>::from_degrees(45.0f);
auto radians = angle.as_radians();

// View angles for camera systems (Source Engine ranges: pitch clamped to +-89, yaw/roll wrapped to +-180)
se::ViewAngles view{
    se::PitchAngle::from_degrees(-10.0f),
    se::YawAngle::from_degrees(90.0f),
    se::RollAngle::from_degrees(0.0f)
};
```

See: [Angle](trigonometry/angle.md), [View Angles](trigonometry/view_angles.md)

### 4. 3D Projection

Built-in camera and projection systems. Cameras are per-engine aliases of the generic `omath::projection::Camera`:

```cpp
namespace se = omath::source_engine;
namespace proj = omath::projection;

proj::ViewPort viewport{1920.0f, 1080.0f};
auto fov = proj::FieldOfView::from_degrees(90.0f);

se::Camera cam(
    omath::Vector3<float>{0, 0, 100},  // Position
    se::ViewAngles{},                  // Angles
    viewport,
    fov,
    0.1f,   // near plane
    1000.0f // far plane
);

// Project 3D point to 2D screen
omath::Vector3<float> world_pos{100, 50, 75};
if (auto screen_pos = cam.world_to_screen(world_pos)) {
    std::cout << "Screen: " << screen_pos->x << ", " << screen_pos->y << "\n";
}
```

`world_to_screen()` returns `std::expected<Vector3<float>, projection::Error>`: `x`/`y` are pixels from the top-left corner and `z` is the normalized depth.

See: [Camera](projection/camera.md), [Error codes](projection/error_codes.md)

### 5. Game Engine Support

OMath provides pre-configured traits for major game engines:

```cpp
// Source Engine
#include <omath/engines/source_engine/camera.hpp>
using SourceCamera = omath::source_engine::Camera;

// Unity Engine
#include <omath/engines/unity_engine/camera.hpp>
using UnityCamera = omath::unity_engine::Camera;

// Unreal Engine (double precision: Vector3<double>, ViewAngles with double angles)
#include <omath/engines/unreal_engine/camera.hpp>
using UnrealCamera = omath::unreal_engine::Camera;

// And more: OpenGL, Frostbite, IW Engine, CryEngine, RAGE, Serious Engine
```

Each engine has its own coordinate system conventions automatically handled.

!!! warning "Do not `using namespace` two namespaces at once"
    `omath::ViewAngles` is a class template and every engine namespace defines an alias with the same name. Writing `using namespace omath;` together with `using namespace omath::source_engine;` makes `ViewAngles` ambiguous. Prefer a namespace alias such as `namespace se = omath::source_engine;`.

See: Engine-specific docs in [engines/](engines/source_engine/camera_trait.md), for example [Source](engines/source_engine/camera_trait.md), [CryEngine](engines/cry_engine/camera_trait.md) and [RAGE](engines/rage_engine/camera_trait.md)

---

## Common Use Cases

### World-to-Screen Projection

```cpp
namespace se = omath::source_engine;

se::Camera cam = /* initialize camera */;
omath::Vector3<float> enemy_position{100, 200, 50};

if (auto screen = cam.world_to_screen(enemy_position)) {
    // Draw ESP box at screen->x, screen->y
    std::cout << "Enemy on screen at: " << screen->x << ", " << screen->y << "\n";
} else {
    // Enemy not visible (behind camera or outside frustum)
}
```

### Projectile Prediction

```cpp
using namespace omath::projectile_prediction;

constexpr Projectile<float> bullet{
    .m_launch_speed = 1000.0f,     // muzzle velocity (units/s)
    .m_gravity_scale = 1.0f,       // multiplier for the engine's gravity constant
};

constexpr Launcher<float> launcher{
    .eye_origin = {0, 0, 64},                                    // camera / eye position
    .muzzle_offset = {.forward = 16.f, .right = 8.f, .up = -6.f}, // where the round spawns, in view space
};

constexpr Target<float> enemy{
    .m_origin = {100, 200, 50},    // position
    .m_velocity = {10, 0, 0},      // velocity
    .m_is_airborne = false,
};

// Calculate the view angles that make the shot connect
const ProjPredEngineLegacy<> engine(/*gravity*/ 800.f, /*step*/ 1.f / 1000.f, /*horizon*/ 10.f, /*tolerance*/ 5.f);
if (const auto aim = engine.maybe_calculate_aim(bullet, launcher, enemy)) {
    // set aim->angles.pitch / yaw on the camera, or draw aim->aim_point
}
```

See: [Projectile Prediction](projectile_prediction/projectile_engine.md)

### Collision Detection

```cpp
#include <omath/collision/line_tracer.hpp>

using omath::Vector3;
using Tracer = omath::collision::LineTracer<>;

// A triangle in the XY plane and a ray shot straight down through it
const omath::Triangle<Vector3<float>> triangle{{0, 0, 0}, {100, 0, 0}, {0, 100, 0}};
const omath::collision::Ray<> ray{.start = {25, 25, 100}, .end = {25, 25, -100}};

// get_ray_hit_point() returns ray.end when nothing is hit
const auto hit = Tracer::get_ray_hit_point(ray, triangle);
if (hit != ray.end) {
    std::cout << "Hit at: " << hit.x << ", " << hit.y << ", " << hit.z << "\n"; // (25, 25, 0)
}
```

`LineTracer` also traces against `Aabb`, `Obb` and meshes. For convex shape overlap and penetration depth use [GJK](collision/gjk_algorithm.md) and [EPA](collision/epa_algorithm.md).

See: [Collision Detection](collision/line_tracer.md)

### Pattern Scanning

```cpp
#include <omath/utility/pattern_scan.hpp>

std::vector<std::byte> memory = /* ... */;
std::span<std::byte> view{memory};

const auto found = omath::PatternScanner::scan_for_pattern(view, "48 8B 05 ?? ?? ?? ?? 48 85 C0");
if (found != view.end()) {
    std::cout << "Pattern found at offset: " << (found - view.begin()) << "\n";
}
```

The result is an iterator, and `end()` means "not found". For whole PE, ELF and Mach-O files or loaded modules see the PE/ELF/Mach-O scanners below.

See: [Pattern Scanning](utility/pattern_scan.md)

---

## Beyond the Basics

| I want to... | Go to |
|--------------|-------|
| Draw boxes, bars, labels, crosshairs on top of a game | [HUD overview](hud/overview.md), [Screen overlay](hud/screen_overlay.md), [Entity overlay](hud/entity_overlay.md) |
| Hook DirectX 9/11/12 or OpenGL present/swap calls | [HooksManager](hooks/hooks_manager.md) (build option `OMATH_ENABLE_HOOKING`) |
| Script OMath from Lua | [Lua scripting](lua/lua.md) (build option `OMATH_ENABLE_LUA`) |
| Read game structures with typed offsets | [External object](rev_eng/external_rev_object.md), [Internal object](rev_eng/internal_rev_object.md), [Memory trait](rev_eng/memory_trait.md) |
| Find signatures in files and loaded modules | [PE](utility/pe_pattern_scan.md), [ELF](utility/elf_pattern_scan.md), [Mach-O](utility/macho_pattern_scan.md), [Scan result](utility/section_scan_result.md) |
| Resolve Unreal Engine actor names | [Actor name](engines/unreal_engine/actor_name.md), [Object array](engines/unreal_engine/object_array.md) |
| Rotate with quaternions | [Quaternion](linear_algebra/quaternion.md) |
| Cull or trace oriented boxes | [AABB](3d_primitives/aabb.md), [OBB](3d_primitives/obb.md) |
| Build a radar or pick the closest target | [Radar](algorithm/radar.md), [Targeting](algorithm/targeting.md) |
| Walk a bot along a navigation mesh | [A*](pathfinding/a_star.md), [Navigation mesh](pathfinding/navigation_mesh.md), [Walk bot](pathfinding/walk_bot.md) |
| Predict a round that has air drag | [Drag engine](projectile_prediction/proj_pred_engine_drag.md), [Projectile flight](projectile_prediction/projectile_flight.md) |

The repository's `examples/` directory contains runnable projects: a barycentric-coordinates demo, a GLFW/OpenGL window that renders a mesh through an OMath camera, a projection matrix builder, a signature scanner, the HUD gallery, DirectX 9/11/12 and OpenGL hook DLLs, and a complete process-attached overlay (`example_kf1_dumper`, `example_kf1_overlay`). Build them with `-DOMATH_BUILD_EXAMPLES=ON`.

---

## Next Steps

Now that you have the basics, explore these topics:

1. **[API Overview](api_overview.md)** - Map of every module
2. **[Tutorials](tutorials.md)** - Step-by-step walkthroughs
3. **[Examples](../examples/)** - Working code examples
4. **[Best Practices](best_practices.md)** - Performance and safety guidelines

---

## Getting Help

- **Documentation**: [http://libomath.org](http://libomath.org)
- **Discord**: [Join our community](https://discord.gg/eDgdaWbqwZ)
- **Telegram**: [@orangennotes](https://t.me/orangennotes)
- **Issues**: [GitHub Issues](https://github.com/orange-cpp/omath/issues)
- **Stuck?** See the [FAQ](faq.md) and [Troubleshooting](troubleshooting.md)
