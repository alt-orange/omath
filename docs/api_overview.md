# API Overview

This document provides a high-level overview of OMath's API, organized by functionality area. Every section links to the detailed reference pages.

---

## Module Organization

OMath is organized into several logical modules:

### Core Mathematics
- **[Linear Algebra](linear_algebra/vector3.md)** - Vectors, matrices, quaternions, triangles
- **[Trigonometry](trigonometry/angle.md)** - Angles, view angles, trigonometric helpers
- **[3D Primitives](3d_primitives/mesh.md)** - Meshes, boxes, planes, [AABB](3d_primitives/aabb.md) and [OBB](3d_primitives/obb.md)

### Game Development
- **[Collision Detection](collision/line_tracer.md)** - Ray tracing against triangles, AABB, OBB and meshes; GJK/EPA for convex shapes
- **[Projectile Prediction](projectile_prediction/projectile_engine.md)** - Ballistics and aim-assist calculations, including air drag
- **[Projection](projection/camera.md)** - Camera systems, world-to-screen transformations, frustum culling
- **[Pathfinding](pathfinding/a_star.md)** - A* algorithm, navigation meshes and a [walk bot](pathfinding/walk_bot.md)
- **[Radar and targeting](algorithm/radar.md)** - `world_to_radar` and [closest-target helpers](algorithm/targeting.md)

### Engine Support
- **Source Engine** - Valve's Source Engine (TF2, CS:GO, etc.)
- **Unity Engine** - Unity game engine
- **Unreal Engine** - Epic's Unreal Engine (double precision, plus [actor name](engines/unreal_engine/actor_name.md) and [object array](engines/unreal_engine/object_array.md) resolution)
- **Frostbite Engine** - EA's Frostbite Engine
- **IW Engine** - Infinity Ward's engine (Call of Duty)
- **CryEngine** - Crytek's CryEngine
- **RAGE Engine** - Rockstar's RAGE
- **Serious Engine** - Croteam's Serious Engine
- **OpenGL Engine** - Canonical OpenGL coordinate system

### Tooling
- **[HUD and overlays](hud/overview.md)** - Declarative ESP/HUD widgets, a renderer interface and an ImGui backend
- **[Hooking](hooks/hooks_manager.md)** - `HooksManager` for DirectX 9/11/12 and OpenGL present hooks (opt-in)
- **[Lua scripting](lua/lua.md)** - Lua bindings for the library (opt-in)
- **[Reverse engineering](rev_eng/external_rev_object.md)** - Typed internal/external memory objects and memory traits
- **[Pattern scanning](utility/pattern_scan.md)** - Wildcard byte patterns in memory, [PE](utility/pe_pattern_scan.md), [ELF](utility/elf_pattern_scan.md) and [Mach-O](utility/macho_pattern_scan.md) files and modules

### Utilities
- **[Color](utility/color.md)** - RGBA color representation with HSV helpers
- **[Hashing](utility/hashing.md)** - `fnv1a`, `crc32`, Base64
- **[Encrypted variable](containers/encrypted_variable.md)** - Compile-time keyed XOR wrapper for values held in memory

### What needs a build option

Most of the library works out of the box. These parts are compiled only on request (see [Installation](install.md#optional-features)):

| Feature | CMake option | Notes |
|---------|--------------|-------|
| AVX2 code paths, `ProjPredEngineAvx2` | `OMATH_USE_AVX2` | Default on when the compiler supports it |
| ImGui conversions, `ImguiHudRenderer` | `OMATH_IMGUI_INTEGRATION` | Links ImGui |
| `HooksManager` | `OMATH_ENABLE_HOOKING` | Windows/Linux x86-64, uses safetyhook |
| Lua bindings | `OMATH_ENABLE_LUA` | Needs Lua and sol2 |
| C++ module (`import omath;`) | `OMATH_ENABLE_MODULES` | CMake 3.28+ |

---

## Core Types

### Vectors

All vector types are template-based and require an arithmetic type. `Vector3` derives from `Vector2`, and `Vector4` from `Vector3`.

| Type | Description | Key Methods |
|------|-------------|-------------|
| `Vector2<T>` | 2D vector | `length()`, `normalized()`, `dot()`, `distance_to()` |
| `Vector3<T>` | 3D vector | `length()`, `length_2d()`, `normalized()`, `dot()`, `cross()`, `angle_between()`, `is_perpendicular()` |
| `Vector4<T>` | 4D vector | Extends Vector3 with a `w` component, `clamp()` |

`length()`, `normalized()` and `distance_to()` need a floating point type; `length_sqr()` and `distance_to_sqr()` work for every arithmetic type.

You may define convenience aliases in your own code:
```cpp
using Vec2f = omath::Vector2<float>;
using Vec3f = omath::Vector3<float>;
using Vec4f = omath::Vector4<float>;
```

**Key features:**
- Component-wise arithmetic (+, -, *, /)
- Scalar multiplication/division
- Dot and cross products
- Safe normalization (returns the vector unchanged if its length is zero)
- Distance calculations
- Angle calculations with error handling
- `std::hash` for use as a key in unordered containers
- `std::formatter` support
- Optional ImGui conversions (`to_im_vec2()`, `to_im_vec4()`) with `OMATH_IMGUI_INTEGRATION`

### Matrices

| Type | Description | Key Methods |
|------|-------------|-------------|
| `Mat<Rows, Columns, Type, StoreType>` | Generic matrix, row- or column-major | `at()`, `operator[row, col]`, `transposed()`, `determinant()`, `inverted()`, `to_string()` |
| `<engine>::Mat4X4` | 4x4 float matrix alias in every engine namespace | Same as above |

`inverted()` returns `std::optional`, empty when the matrix is singular. Helper functions build common transforms: `mat_translation`, `mat_scale`, `mat_rotation_axis_x/y/z`, `mat_rotation_zyx/yxz/zxy`, `mat_look_at_left_handed`, `mat_look_at_right_handed`, `mat_camera_view`, and `mat_extract_origin/scale/rotation_*` take them apart again.

**Use cases:**
- Transformation matrices
- View matrices
- Projection matrices
- Model-view-projection pipelines

### Quaternions

`Quaternion<T>` stores `x, y, z, w` (identity by default) and offers `from_axis_angle()`, the Hamilton product, `conjugate()`, `inverse()`, `normalized()`, `rotate(Vector3)` and `to_rotation_matrix3()` / `to_rotation_matrix4()`. See [Quaternion](linear_algebra/quaternion.md).

### Angles

Strong-typed angle system with compile-time range enforcement:

| Type | Description |
|------|-------------|
| `omath::Angle<T, Min, Max, Flags>` | Generic angle in degrees with bounds. `AngleFlags::Normalized` wraps into the range, `AngleFlags::Clamped` clamps |
| `<engine>::PitchAngle` | Vertical camera rotation. Source and IW Engine clamp to [-89, 89], the other engines to [-90, 90] |
| `<engine>::YawAngle` | Horizontal rotation, wrapped to [-180, 180] |
| `<engine>::RollAngle` | Camera roll, wrapped to [-180, 180] |
| `omath::ViewAngles<Pitch, Yaw, Roll>` | Composite pitch/yaw/roll, `<engine>::ViewAngles` is the instantiation for that engine |
| `projection::FieldOfView` | `Angle<float, 0, 180, Clamped>` |

**Features:**
- Automatic normalization/clamping based on flags
- Conversions between degrees and radians, `from_asin/acos/atan/atan2`, `sin/cos/tan/cot`
- Type-safe arithmetic (`+`, `-`, unary `-`, comparison)
- Prevents common angle bugs

`omath::angles` has free functions such as `radians_to_degrees`, `degrees_to_radians`, `horizontal_fov_to_vertical`, `vertical_fov_to_horizontal` and `wrap_angle`.

---

## Projection System

### Camera

Generic camera template that works with any engine trait:

```cpp
namespace omath::projection {
    template<class Mat4X4Type, class ViewAnglesType, class TraitClass,
             NDCDepthRange depth_range = NDCDepthRange::NEGATIVE_ONE_TO_ONE,
             CameraAxes axes = {}, class NumericType = float>
    class Camera;
}
```

**Engine-specific cameras:**
```cpp
omath::source_engine::Camera      // Source Engine
omath::unity_engine::Camera       // Unity
omath::unreal_engine::Camera      // Unreal (double precision)
omath::frostbite_engine::Camera   // Frostbite
omath::iw_engine::Camera          // IW Engine
omath::cry_engine::Camera         // CryEngine
omath::rage_engine::Camera        // RAGE
omath::serious_engine::Camera     // Serious Engine
omath::opengl_engine::Camera      // OpenGL
```

**Core methods:**
- `world_to_screen(Vector3)` - Project a 3D point to 2D screen space, returns `std::expected<Vector3, projection::Error>`
- `world_to_screen_unclipped(Vector3)` - Same, but keeps points outside the viewport
- `screen_to_world(Vector3 | Vector2)` and `view_port_to_world(ndc)` - Unproject
- `get_view_matrix()`, `get_projection_matrix()`, `get_view_projection_matrix()` - Cached matrices
- `set_origin()`, `set_view_angles()`, `set_field_of_view()`, `set_view_port()`, `set_near_plane()`, `set_far_plane()` - Update camera state
- `look_at(target)`, `calc_look_at_angles(target)` - Aiming helpers
- `get_forward()`, `get_right()`, `get_up()` and their `get_abs_*` variants
- `is_culled_by_frustum(triangle)`, `is_aabb_culled_by_frustum(aabb)`, `is_obb_culled_by_frustum(obb)` - Frustum culling
- `calc_view_angles_from_view_matrix()`, `calc_origin_from_view_matrix()`, `extract_projection_params()` - Recover camera data from game matrices

**Supporting types:**
- `ViewPort` - Screen dimensions (`m_width`, `m_height`) and `aspect_ratio()`
- `FieldOfView` - FOV in degrees, clamped to [0, 180]
- `projection::Error` - Error codes for projection failures

---

## Collision Detection

### GJK/EPA Algorithms

Advanced convex shape collision detection using the Gilbert-Johnson-Keerthi and Expanding Polytope algorithms:

```cpp
namespace omath::collision {
    template<class ColliderInterfaceType>
    class GjkAlgorithm;

    template<class ColliderInterfaceType>
    class Epa;
}
```

**GJK (Gilbert-Johnson-Keerthi):**
* `GjkAlgorithm<C>::is_collide(a, b)` detects collision between two convex shapes
* `is_collide_with_simplex_info(a, b)` also returns the final simplex when a collision is found
* Works with any collider implementing `ColliderInterface`, that is `find_abs_furthest_vertex_position()`, `get_origin()` and `set_origin()`

**EPA (Expanding Polytope Algorithm):**
* `Epa<C>::solve(a, b, simplex, params)` computes penetration depth and separation normal
* Takes GJK's output simplex as input
* Provides contact information for physics simulation
* Configurable iteration limit and convergence tolerance through `Epa::Params`

**Supporting Types:**

| Type | Description | Key Features |
|------|-------------|--------------|
| `ColliderInterface<VectorType>` | Abstract collider | Support function and origin accessors, see [Collider interface](collision/collider_interface.md) |
| `Simplex<VectorType>` | 1-4 point geometric simplex | Fixed capacity, GJK iteration support |
| `MeshCollider<MeshType>` | Convex mesh collider | Support function for GJK/EPA, each engine aliases it (`source_engine::MeshCollider`, `serious_engine::MeshCollider`) |
| `GjkHitInfo<VertexType>` | Collision result | `hit` flag and `simplex` |
| `Epa::Result` | Penetration info | `depth`, `normal`, `penetration_vector`, iteration counts |

### LineTracer

Ray-casting and line tracing utilities:

```cpp
namespace omath::collision {
    template<class T = Vector3<float>> class Ray;           // start, end, infinite_length
    template<class RayType = Ray<>>   class LineTracer;     // static functions only
}
```

**Features:**
- Ray-triangle intersection (Moller-Trumbore algorithm)
- Ray-AABB and ray-OBB intersection (slab method)
- Ray-mesh intersection, traced in the mesh's local space
- `get_ray_hit_point(ray, shape)` returns the hit point, or `ray.end` when nothing was hit
- `can_trace_line(ray, triangle)` is true when nothing blocks the segment

### 3D Primitives

| Type | Description | Key Methods |
|------|-------------|-------------|
| `primitives::Aabb<T, UpAxis>` | Axis-aligned bounding box | `center()`, `extents()`, `top()`, `bottom()`, `vertices()`, `is_collide()` |
| `primitives::Obb<T>` | Oriented bounding box (center, three axes, half extents) | `vertices()` |
| `primitives::Mesh<...>` | Polygonal mesh with transforms | `set_origin()`, `set_rotation()`, `set_scale()`, `vertex_position_to_world_space()`, `make_face_in_world_space()`, `local_bounds()` |
| `primitives::create_box<BoxMesh>()` | Builds a box mesh from a top/bottom axis | See [Box](3d_primitives/box.md) |
| `primitives::create_plane<PlaneMesh>()` | Builds a plane mesh | See [Plane](3d_primitives/plane.md) |

**Mesh Features:**
* Vertex buffer (VBO) and index buffer (VAO/EBO) storage
* Position, rotation, and scale transformations
* Cached transformation matrix
* Engine-specific coordinate system support (`source_engine::Mesh`, `unity_engine::BoxMesh`, ...)
* Compatible with `MeshCollider` for collision detection

---

## Projectile Prediction

All engines live in `omath::projectile_prediction` and share one interface. See [Projectile prediction](projectile_prediction/projectile_engine.md).

### Interfaces

**`ProjPredEngineInterface<T>`** - Base interface for all prediction engines

```cpp
virtual std::optional<AimSolution<T>>
maybe_calculate_aim(const Projectile<T>&, const Launcher<T>&, const Target<T>&) const = 0;

// Compatibility wrappers, kept for one release
std::optional<Vector3<T>>   maybe_calculate_aim_point (const Projectile<T>&, const Target<T>&) const;
std::optional<AimAngles<T>> maybe_calculate_aim_angles(const Projectile<T>&, const Target<T>&) const;
```

`AimSolution<T>` holds `angles` (`AimAngles` in degrees, pitch positive up), `aim_point`, `predicted_target_position` and `time_of_flight`. `Arc::LOW` (flat) or `Arc::HIGH` (lob) selects which pitch is solved.

### Implementations

| Engine | Description | Notes |
|--------|-------------|-------|
| `ProjPredEngineLegacy<Trait, T>` | Time scan with closed-form pitch, trait-driven | Portable, works with every engine trait, see [Legacy](projectile_prediction/proj_pred_engine_legacy.md) |
| `ProjPredEngineAvx2` | AVX2 scan, `float`, Source-style axes | Needs `OMATH_USE_AVX2` on x86, every call throws otherwise, see [AVX2](projectile_prediction/proj_pred_engine_avx2.md) |
| `ProjPredEngineDrag<Trait, T>` | Flies the round through `ProjectileFlight` | For rounds with air drag or a speed cap, see [Drag](projectile_prediction/proj_pred_engine_drag.md) |

All of them satisfy the same interface and can be swapped. `PredEngineConcept` ([concept](projectile_prediction/pred_engine_concept.md)) defines what an engine trait must provide.

### Supporting Types

**`Projectile`** - Defines the round itself:
```cpp
template<class T = float>
class Projectile {
public:
    T m_launch_speed;    // muzzle velocity
    T m_gravity_scale;   // multiplier for the engine's gravity constant
    T m_drag, m_drag_forward, m_drag_up;  // air drag, used by ProjPredEngineDrag / ProjectileFlight
    T m_max_speed;       // speed cap of the physics engine, 0 = none
    // m_origin / m_launch_offset: read only by the compatibility wrappers
};
```

**`Launcher`** - Who fires, and from where:
```cpp
template<class T = float>
struct Launcher {
    Vector3<T>      eye_origin;      // where the view angles apply
    MuzzleOffset<T> muzzle_offset;   // forward / right / up, rotates with the view
    Vector3<T>      world_offset;    // fixed world-space part, usually zero
    T               launch_pitch_offset; // degrees the round leaves above the crosshair (TF2 pipes: 9.46)
};
```

**`Target`** - Defines target state:
```cpp
template<class T = float>
class Target {
public:
    Vector3<T> m_origin;
    Vector3<T> m_velocity;
    bool       m_is_airborne{};  // airborne targets fall under gravity
};
```

---

## Pathfinding

### A* Algorithm

```cpp
namespace omath::pathfinding {
    class Astar {
    public:
        static std::vector<Vector3<float>>
        find_path(const Vector3<float>& start, const Vector3<float>& end, const NavigationMesh& nav_mesh) noexcept;
    };
}
```

**Features:**
- Works on a `NavigationMesh` of `Vector3<float>` vertices
- Efficient priority queue implementation
- Path reconstruction

### Navigation Mesh

```cpp
namespace omath::pathfinding {
    class NavigationMesh;
}
```

**Features:**
- Vertex graph with neighbor lists (`m_vertex_map`, `get_neighbors()`)
- `get_closest_vertex()` to snap a point onto the mesh
- Per-vertex event tags (`set_event()`, `get_event()`, for example "jump")
- `serialize()` / `deserialize()`

### Walk Bot

`WalkBot` follows a path across a navigation mesh. Call `set_target()` once, `update(bot_position)` every tick, and receive the next waypoint through `on_path()` and status changes (`IDLE`, `PATHING`, `FINISHED`) through `on_status()`. See [Walk bot](pathfinding/walk_bot.md).

---

## Radar and Targeting

`omath::algorithm` (headers `omath/algorithm/radar.hpp` and `omath/algorithm/targeting.hpp`, not included by `omath.hpp`):

- `world_to_radar(camera, position, scale)` - 2D radar offset of a world position relative to the camera, rotated by its yaw
- `get_closest_target_by_fov(...)` - pick the target whose direction is closest to the current view angles
- `get_closest_target_by_distance(...)` - pick the closest target in world space

See [Radar](algorithm/radar.md) and [Targeting](algorithm/targeting.md).

---

## HUD and Overlays

`omath::hud` describes what to draw as small widget structs and turns them into primitive calls on a `HudRendererInterface`:

| Class | Purpose |
|-------|---------|
| `EntityOverlay` | Widgets around one entity's 2D box (boxes, bars, labels, skeleton, snap lines), built with `EntityOverlay::from_aabb(camera, aabb, aspect, renderer)` |
| `ScreenOverlay` | Widgets relative to the screen (crosshair, FOV circle, hit marker, threat arrow, corner text) |
| `HudRendererInterface` | The primitive drawing operations a backend implements |
| `ImguiHudRenderer` | Ready-made backend drawing into ImGui, available with `OMATH_IMGUI_INTEGRATION` |
| `Gradient`, `widget::*` | Paints and widget descriptors |

Both overlays accept a declarative `contents(...)` list. See [HUD overview](hud/overview.md), [Widgets](hud/widgets.md), [Renderer interface](hud/renderer_interface.md) and [ImGui renderer](hud/imgui_renderer.md).

---

## Hooking

`omath::hooks::HooksManager` (singleton, `HooksManager::get()`) finds and hooks the present/swap functions of DirectX 9, 11, 12 and OpenGL and calls your callbacks every frame, so the HUD can draw into the game's render loop. It also hooks the window procedure for input. It exists only with `OMATH_ENABLE_HOOKING`; without it the class is an empty stub. Typical flow:

```cpp
auto& hooks = omath::hooks::HooksManager::get();
hooks.set_on_present([](IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) { /* draw */ });
if (!hooks.hook_dx11()) { /* failed */ }
```

See [HooksManager](hooks/hooks_manager.md).

---

## Lua Scripting

With `OMATH_ENABLE_LUA`, `omath::lua::LuaInterpreter::register_lib(lua_State*)` creates a global `omath` table with vectors, matrices, quaternions, colors, collision, pattern scanning, the HUD and one sub-table per engine camera. See [Lua scripting](lua/lua.md).

---

## Engine Traits

Each game engine has a trait system providing engine-specific math:

### CameraTrait

Implements camera math for an engine:
- `calc_look_at_angle()` - Calculate angles to look at a point
- `calc_view_matrix()` - Build view matrix from angles and position
- `calc_projection_matrix()` - Build projection matrix from FOV, viewport, near/far planes and NDC depth range
- Optional `extract_rotation_angles()` and `calc_field_of_view()` - used by `Camera` to recover data from matrices

### MeshTrait

Provides mesh transformation for an engine:
- `rotation_matrix()` - Build rotation matrix from engine-specific angles
- Handles coordinate system differences (Y-up vs Z-up, left/right-handed)
- Used by `Mesh` class for local-to-world transformations

### PredEngineTrait

Provides physics/ballistics specific to an engine (the `PredEngineConcept`):
- Projectile and target position prediction (gravity applied along the engine's up axis)
- Horizontal distance and height coordinate
- View basis for a pitch/yaw
- Direct pitch/yaw angles between two points

### Available Traits

| Engine | Namespace | Up / Right / Forward | Pitch range | Depth range | Scalar |
|--------|-----------|----------------------|-------------|-------------|--------|
| Source Engine | `source_engine` | +Z / -Y / +X | [-89, 89] | 0..1 | `float` |
| Unity Engine | `unity_engine` | +Y / +X / +Z | [-90, 90] | 0..1 | `float` |
| Unreal Engine | `unreal_engine` | +Z / +Y / +X | [-90, 90] | 0..1 | `double` |
| Frostbite | `frostbite_engine` | +Y / +X / +Z | [-90, 90] | 0..1 | `float` |
| IW Engine | `iw_engine` | +Z / -Y / +X | [-89, 89] | 0..1 | `float` |
| CryEngine | `cry_engine` | +Z / +X / +Y | [-90, 90] | 0..1 | `float` |
| RAGE | `rage_engine` | +Z / +X / +Y | [-90, 90] | 0..1 | `float` |
| Serious Engine | `serious_engine` | +Y / +X / -Z | [-90, 90] | 0..1 | `float` |
| OpenGL | `opengl_engine` | +Y / +X / -Z | [-90, 90] | -1..1 | `float` |

All nine engines provide a camera trait, mesh trait, prediction engine trait, constants and formulas.

**Documentation:**
- See `docs/engines/<engine_name>/` for detailed per-engine docs: [Source](engines/source_engine/camera_trait.md), [Unity](engines/unity_engine/camera_trait.md), [Unreal](engines/unreal_engine/camera_trait.md), [Frostbite](engines/frostbite/camera_trait.md), [IW Engine](engines/iw_engine/camera_trait.md), [CryEngine](engines/cry_engine/camera_trait.md), [RAGE](engines/rage_engine/camera_trait.md), [Serious Engine](engines/serious_engine/camera_trait.md), [OpenGL](engines/opengl_engine/camera_trait.md)
- Each engine has separate docs for camera_trait, mesh_trait, pred_engine_trait, constants, and formulas

---

## Utility Functions

### Color

```cpp
class Color {
public:
    Color(float r, float g, float b, float a);         // 0..1, clamped
    static Color from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    static Color from_hsv(float hue, float saturation, float value);
    Hsv to_hsv() const;

    // Blending
    Color blend(const Color& other, float ratio) const;

    // Presets
    static Color red(); static Color green(); static Color blue();
};
```

See [Color](utility/color.md).

### Pattern Scanning

**Binary pattern search with wildcards:**

```cpp
#include <omath/utility/pattern_scan.hpp>

std::span<std::byte> memory_view{memory_buffer};

// Pattern with wildcards (?? or ? = any byte)
auto it = omath::PatternScanner::scan_for_pattern(memory_view, "48 8B 05 ?? ?? ?? ?? 48 85 C0");
if (it != memory_view.end()) {
    std::cout << "Found at offset: " << (it - memory_view.begin()) << "\n";
}
```

**PE, ELF and Mach-O scanning:**

```cpp
#include <omath/utility/pe_pattern_scan.hpp>

// PePatternScanner, ElfPatternScanner and MachOPatternScanner share the same three entry points:
// scan_for_pattern_in_file, scan_for_pattern_in_loaded_module and scan_for_pattern_in_memory_file
if (auto result = omath::PePatternScanner::scan_for_pattern_in_file("target.exe", "E8 ?? ?? ?? ??")) {
    // SectionScanResult: virtual_base_addr, raw_base_addr, target_offset
    std::cout << "Found at 0x" << std::hex << (result->virtual_base_addr + result->target_offset) << "\n";
}
```

Each also has a compile-time form, `scan_for_pattern_in_file<"E8 ?? ?? ?? ??">(path)`. The default section is `.text` (`__text` for Mach-O). The file scanners parse the binary format themselves, so a PE can be scanned on Linux and an ELF on Windows. See [Pattern scan](utility/pattern_scan.md), [PE](utility/pe_pattern_scan.md), [ELF](utility/elf_pattern_scan.md), [Mach-O](utility/macho_pattern_scan.md) and [Scan result](utility/section_scan_result.md).

### Reverse Engineering

Reversed structures get typed, named accessors instead of raw offsets. The same class shape works from outside a process (`ExternalReverseEngineeredObject`, backed by a memory trait you write around `ReadProcessMemory`/`process_vm_readv`/a driver) and from inside it (`InternalReverseEngineeredObject`):

```cpp
struct MyTrait {
    template<class T> static T read_memory(std::uintptr_t address);
    template<class T> static void write_memory(std::uintptr_t address, const T& value);
};

class Player final : public omath::rev_eng::ExternalReverseEngineeredObject<MyTrait> {
public:
    using ExternalReverseEngineeredObject::ExternalReverseEngineeredObject;

    [[nodiscard]] omath::Vector3<float> origin() const { return get_by_offset<omath::Vector3<float>>(0x134); }
    [[nodiscard]] int health() const { return get_by_offset<int>(0x140); }
};

Player local_player{local_player_address};
```

`InternalReverseEngineeredObject` returns references into the object and adds `call_method`, `call_virtual_method` and pattern-resolved calls. `rev_eng::InternalMemoryTrait`, the `MemoryReadTrait` concept and `read_pointer` live in `omath/rev_eng/memory_trait.hpp`. See [External object](rev_eng/external_rev_object.md), [Internal object](rev_eng/internal_rev_object.md) and [Memory trait](rev_eng/memory_trait.md).

### Hashing and Encrypted Variables

- `omath::hashing::fnv1a()` and `crc32()` are `constexpr` string hashes, `base64_encode()` / `base64_decode()` convert Base64 (decode returns `std::expected<std::string, Base64Error>`). See [Hashing](utility/hashing.md).
- `OMATH_DEF_CRYPT_VAR(TYPE, KEY_SIZE)` declares an `EncryptedVariable` whose bytes are XORed with a key generated at compile time; `drop_anchor()` decrypts for the lifetime of the returned anchor. See [Encrypted variable](containers/encrypted_variable.md).

---

## Concepts and Constraints

OMath uses C++20 concepts and constraints for type safety:

```cpp
// Vectors, matrices, quaternions accept arithmetic types only
template<class Type> requires std::is_arithmetic_v<Type> class Vector3;

// Camera engine traits (simplified)
template<class T, class MatType, class ViewAnglesType, class NumericType>
concept CameraEngineConcept = requires(const Vector3<NumericType>& origin, const Vector3<NumericType>& look_at,
                                       const ViewAnglesType& angles, const FieldOfView& fov, const ViewPort& viewport,
                                       NumericType z_near, NumericType z_far, NDCDepthRange depth_range) {
    { T::calc_look_at_angle(origin, look_at) } noexcept -> std::same_as<ViewAnglesType>;
    { T::calc_view_matrix(angles, origin) } noexcept -> std::same_as<MatType>;
    { T::calc_projection_matrix(fov, viewport, z_near, z_far, depth_range) } noexcept -> std::same_as<MatType>;
};
```

The other concepts are `PredEngineConcept<T, ArithmeticType>` (prediction traits, see [Pred engine concept](projectile_prediction/pred_engine_concept.md)), `MemoryReadTrait` (memory traits) and the `GjkVector`/`EpaVector` constraints on collision vectors.

---

## Error Handling

OMath uses modern C++ error handling:

### std::expected (C++23)

```cpp
// omath::Vector3<float>::angle_between
std::expected<Angle<float, 0.f, 180.f, AngleFlags::Clamped>, Vector3Error>
angle_between(const Vector3& other) const;

if (auto angle = v1.angle_between(v2)) {
    // Success: use *angle
} else {
    // Error: angle.error() gives Vector3Error::IMPOSSIBLE_BETWEEN_ANGLE
}
```

### Projection results

```cpp
std::expected<Vector3<float>, projection::Error>
world_to_screen(const Vector3<float>& world) const;

if (auto screen = camera.world_to_screen(pos)) {
    // Success: use screen->x, screen->y
} else {
    // Point not visible: screen.error() says why
}
```

### std::optional

Operations that can simply "not find" something return `std::optional`: `Mat::inverted()`, `maybe_calculate_aim()`, `Epa::solve()`, the PE/ELF/Mach-O scans and `NavigationMesh::get_event()`.

### Error Codes

```cpp
enum class projection::Error : uint16_t {
    WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS,
    INV_VIEW_PROJ_MAT_DET_EQ_ZERO,
    PERSPECTIVE_DIVIDER_LESS_EQ_ZERO,   // point behind the camera
};
```

Other error types are `hud::EntityOverlayError`, `hashing::Base64Error` and `pathfinding` string errors from `get_closest_vertex()`.

---

## Performance Considerations

### constexpr Support

Most operations are `constexpr` where possible:

```cpp
constexpr omath::Vector3<float> v{1, 2, 3};
constexpr auto len_sq = v.length_sqr();  // Computed at compile time
```

### AVX2 Optimizations

With `OMATH_USE_AVX2` the matrix multiply, the pattern scanner and `ProjPredEngineAvx2` use AVX2:

```cpp
using namespace omath::projectile_prediction;

// Portable, trait-driven
ProjPredEngineLegacy<> legacy_engine(800.f, 1.f / 1000.f, 10.f, 5.f);

// Vectorised scan (float, Source-style axes, AVX2 builds only)
ProjPredEngineAvx2 fast_engine(800.f, 1.f / 1000.f, 10.f);
```

**When to use AVX2:**
- x86-64 builds with `OMATH_USE_AVX2` on
- Performance-critical paths
- Batch operations

**When to use Legacy:**
- ARM platforms and builds without AVX2
- Any engine other than Source-style axes
- Guaranteed compatibility

### Cache Efficiency

```cpp
// Good: contiguous storage
std::vector<omath::Vector3<float>> positions;

// Good: structure of arrays for SIMD
struct Particles {
    std::vector<float> x, y, z;
};
```

---

## Platform Support

| Platform | Support | Notes |
|----------|---------|-------|
| Windows | yes | MSVC, Clang, MinGW |
| Linux | yes | GCC, Clang |
| macOS | yes | Clang |

Other presets exist for ARM, Android, iOS, wasm and FreeBSD. The hooks are limited to Windows and Linux (x86/x86-64).

**Minimum requirements:**
- A C++23 compiler (the project documents Clang 18+, GCC 14+, MSVC 19.38+)
- CMake 3.26+ to build from source

---

## Thread Safety

- **Vector/Matrix types**: Plain value types, safe to share when only read
- **Camera**: Not thread-safe. It caches matrices lazily behind `const` getters, so even read-only use from several threads races. Give each thread its own camera
- **Pattern scanning**: Safe on read-only buffers
- **Memory access**: Depends on OS/process synchronization
- **HooksManager**: Callbacks are guarded by shared mutexes, but the callbacks themselves run on the game's render thread

**Thread-safe example:**
```cpp
// Safe: each thread gets its own camera
std::vector<std::thread> threads;
for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([i]() {
        omath::source_engine::Camera camera = /* create camera */;
        // Use camera in this thread
    });
}
```

---

## Best Practices

### 1. Use Type Aliases

```cpp
using Vec3f = omath::Vector3<float>;
using Mat4 = omath::source_engine::Mat4X4;
```

### 2. Prefer constexpr When Possible

```cpp
constexpr auto compute_at_compile_time() {
    omath::Vector3<float> v{1, 2, 3};
    return v.length_sqr();
}
```

### 3. Check Optional/Expected Results

```cpp
// Good
if (auto result = camera.world_to_screen(pos)) {
    use(*result);
}

// Bad - undefined behavior if the projection failed
auto result = camera.world_to_screen(pos);
use(result->x);
```

### 4. Use Engine-Specific Types

```cpp
// Good: uses correct coordinate system
namespace se = omath::source_engine;
se::Camera camera = /* ... */;

// Bad: mixing engine types - does not compile, and would be the wrong convention anyway
omath::unity_engine::Camera unity_camera{pos, se::ViewAngles{}, /* ... */};
```

---

## See Also

- [Getting Started Guide](getting_started.md)
- [Installation Instructions](install.md)
- [Tutorials](tutorials.md)
- [Best Practices](best_practices.md)
- [Examples Directory](../examples/)
- Individual module documentation in respective folders
