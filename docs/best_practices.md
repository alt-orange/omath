# Best Practices

Guidelines for using OMath effectively and avoiding common pitfalls.

---

## Code Organization

### Use Type Aliases

Define clear type aliases for commonly used types. Matrices and angles belong to an engine, so alias the engine's:

```cpp
// Good: Clear and concise
using Vec3f = omath::Vector3<float>;
using Vec2f = omath::Vector2<float>;
using Mat4 = omath::source_engine::Mat4X4;

Vec3f position{1.0f, 2.0f, 3.0f};
```

```cpp
// Avoid: Verbose and repetitive
omath::Vector3<float> position{1.0f, 2.0f, 3.0f};
omath::Vector3<float> velocity{0.0f, 0.0f, 0.0f};
```

### Namespace Usage

Be selective with `using namespace`:

```cpp
// Good: a namespace alias for your engine
namespace se = omath::source_engine;

// Good: Import specific types
using omath::Vector3;
using omath::Vector2;

// Avoid: Too broad
using namespace omath;  // Imports everything

// Bad: ambiguous - omath::ViewAngles (template) vs source_engine::ViewAngles (alias)
using namespace omath;
using namespace omath::source_engine;
```

### Include What You Use

```cpp
// Good: Include specific headers
#include <omath/linear_algebra/vector3.hpp>
#include <omath/projection/camera.hpp>
#include <omath/engines/source_engine/camera.hpp>

// Okay for development
#include <omath/omath.hpp>

// Production: Include only what you need
// to reduce compile times
```

`omath.hpp` does not include the radar and targeting algorithms, the walk bot, AABB/OBB/mesh primitives, per-engine meshes, ELF/Mach-O scanners, the HUD, the hooks or the Lua bindings. Include those headers yourself.

---

## Error Handling

### Always Check Expected Results

```cpp
// Good: Check before using
if (auto screen = camera.world_to_screen(world_pos)) {
    draw_at(screen->x, screen->y);
} else {
    // Handle point not visible: screen.error() says why
}

// Bad: Unchecked access is undefined behavior if the projection failed!
auto screen = camera.world_to_screen(world_pos);
draw_at(screen->x, screen->y);
```

### Handle Expected Errors

```cpp
// Good: Handle error case
if (auto angle = v1.angle_between(v2)) {
    use_angle(*angle);
} else {
    switch (angle.error()) {
        case omath::Vector3Error::IMPOSSIBLE_BETWEEN_ANGLE:
            // Handle zero-length vector
            break;
    }
}

// Bad: Assume success
auto angle = v1.angle_between(v2);
use_angle(*angle);  // Undefined behavior if there was an error
```

`std::expected::value()` throws `std::bad_expected_access`, while `operator*` and `operator->` on an error are undefined behavior.

### Validate Inputs

```cpp
using namespace omath::projectile_prediction;

// Good: Validate before expensive operations
bool is_valid_projectile(const Projectile<float>& proj) {
    return proj.m_launch_speed > 0.0f &&
           std::isfinite(proj.m_launch_speed) &&
           std::isfinite(proj.m_gravity_scale);
}

bool is_valid_target(const Target<float>& target) {
    return std::isfinite(target.m_origin.length_sqr()) &&
           std::isfinite(target.m_velocity.length_sqr());
}

if (is_valid_projectile(proj) && is_valid_target(target)) {
    const auto aim = engine.maybe_calculate_aim(proj, launcher, target);
}
```

### Check Optional Results Too

`Mat::inverted()`, `maybe_calculate_aim()`, `Epa::solve()` and the pattern scans of loaded modules and files return `std::optional`. An empty result is a normal outcome, not an exception.

---

## Performance

### Use constexpr When Possible

```cpp
// Good: Computed at compile time
constexpr omath::Vector3<float> gravity{0.0f, 0.0f, -9.81f};
constexpr float max_range = 1000.0f;
constexpr float max_range_sq = max_range * max_range;

// Use in runtime calculations
if (position.length_sqr() < max_range_sq) {
    // ...
}
```

### Prefer Squared Distance

```cpp
// Good: Avoids expensive sqrt
constexpr float max_dist_sq = 100.0f * 100.0f;
for (const auto& entity : entities) {
    if (entity.pos.distance_to_sqr(player_pos) < max_dist_sq) {
        // Process nearby entity
    }
}

// Avoid: Unnecessary sqrt calls
constexpr float max_dist = 100.0f;
for (const auto& entity : entities) {
    if (entity.pos.distance_to(player_pos) < max_dist) {
        // More expensive
    }
}
```

### Cache Expensive Calculations

```cpp
// Good: Update camera once per frame
void update_frame() {
    camera.set_origin(current_position);
    camera.set_view_angles(current_angles);

    // All projections use cached matrices
    for (const auto& entity : entities) {
        if (auto screen = camera.world_to_screen(entity.pos)) {
            draw_entity(screen->x, screen->y);
        }
    }
}

// Bad: Camera recreated each call
for (const auto& entity : entities) {
    se::Camera cam(pos, angles, viewport, fov, near_plane, far_plane);  // Throws the cached matrices away!
    auto screen = cam.world_to_screen(entity.pos);
}
```

Setters invalidate only what they change: `set_origin()` / `set_view_angles()` rebuild the view matrix, `set_field_of_view()` / `set_view_port()` / `set_near_plane()` / `set_far_plane()` the projection matrix. Do not call them with unchanged values every frame if you can avoid it.

### Cull Before You Project

```cpp
// Good: reject boxes outside the frustum with six plane tests
if (camera.is_aabb_culled_by_frustum(entity_box))
    continue;
```

### Choose the Right Engine

AVX2 is a build-time choice: the `omath` target is compiled with `-mavx2`/`/arch:AVX2` when `OMATH_USE_AVX2` is on, and the macro is visible to your code. Pick the prediction engine through the common interface:

```cpp
using namespace omath::projectile_prediction;

std::unique_ptr<ProjPredEngineInterface<float>> make_engine()
{
#ifdef OMATH_USE_AVX2
    // float, Source-style axes (z up) only
    return std::make_unique<ProjPredEngineAvx2>(800.f, 1.f / 1000.f, 10.f);
#else
    return std::make_unique<ProjPredEngineLegacy<>>(800.f, 1.f / 1000.f, 10.f, 5.f);
#endif
}
```

`ProjPredEngineAvx2` throws `std::runtime_error` when the build has no AVX2, so never pick it based on the CPU alone. For other engine traits, `double`, or air drag use `ProjPredEngineLegacy<Trait>` / `ProjPredEngineDrag<Trait>`.

### Minimize Allocations

```cpp
// Good: Reuse vectors
std::vector<omath::Vector3<float>> positions;
positions.reserve(expected_count);

// In loop
positions.clear();  // Doesn't deallocate
for (...) {
    positions.push_back(compute_position());
}

// Bad: Allocate every time
for (...) {
    std::vector<omath::Vector3<float>> positions;  // Allocates each iteration
    // ...
}
```

### Keep the Render Callback Light

If you draw through [HooksManager](hooks/hooks_manager.md) your callback runs on the game's render thread every frame. Create contexts and textures on the first call, then only draw: no file I/O, no blocking waits and no per-frame allocation of big containers. Release render targets in the resize callback, and unhook when your module unloads.

---

## Type Safety

### Use Strong Angle Types

```cpp
namespace se = omath::source_engine;

// Good: Type-safe angles
se::PitchAngle pitch = se::PitchAngle::from_degrees(45.0f);
se::YawAngle yaw = se::YawAngle::from_degrees(90.0f);
se::ViewAngles angles{pitch, yaw, se::RollAngle::from_degrees(0.0f)};

// Bad: Raw floats lose safety
float pitch = 45.0f;  // No range checking
float yaw = 90.0f;    // Can go out of bounds
```

### Match Engine Types

```cpp
// Good: Use matching types from same engine
namespace se = omath::source_engine;
se::Camera camera = /* ... */;
se::ViewAngles angles = /* ... */;

// Bad: Mixing engine types - the camera's trait expects its own ViewAngles type, this does not compile
omath::unity_engine::Camera unity_camera{pos, se::ViewAngles{}, /* ... */};
```

Unreal is the exception to "everything is `float`": its camera, angles and constants use `double`.

### Template Type Parameters

```cpp
// Good: Explicit and clear
omath::Vector3<float> position;
omath::Vector3<double> high_precision_pos;

// Avoid: Mixing types unintentionally
omath::Vector3<float> a;
omath::Vector3<double> b;
auto result = a + b;                                        // Does not compile
auto fixed = a + static_cast<omath::Vector3<float>>(b);     // Explicit conversion
```

There is no default template argument for `Vector3`, always spell the type: `Vector3<float>`.

---

## Testing & Validation

### Test Edge Cases

```cpp
void test_projection() {
    se::Camera camera = setup_camera();
    const auto forward = camera.get_abs_forward();

    // Test normal case: a point straight ahead
    assert(camera.world_to_screen(camera.get_origin() + forward * 100.0f).has_value());

    // Test edge cases
    assert(!camera.world_to_screen(camera.get_origin() - forward * 100.0f).has_value());  // Behind
    assert(!camera.world_to_screen(camera.get_origin() + forward * 100.0f
                                   + camera.get_abs_right() * 100000.0f).has_value());     // Off screen

    // Unclipped projection keeps off-screen points but still rejects points behind the camera
    assert(camera.world_to_screen_unclipped(camera.get_origin() + forward * 100.0f
                                            + camera.get_abs_right() * 100000.0f).has_value());
}
```

### Validate Assumptions

```cpp
void validate_game_data() {
    // Validate FOV
    float fov = read_game_fov();
    assert(fov > 1.0f && fov < 179.0f);

    // Validate positions
    omath::Vector3<float> pos = read_player_position();
    assert(std::isfinite(pos.x));
    assert(std::isfinite(pos.y));
    assert(std::isfinite(pos.z));

    // Validate viewport
    omath::projection::ViewPort vp = read_viewport();
    assert(vp.m_width > 0 && vp.m_height > 0);
}
```

### Use Assertions

```cpp
// Good: Catch errors early in development
void shoot_projectile(const omath::projectile_prediction::Projectile<float>& proj) {
    assert(proj.m_launch_speed > 0.0f && "Projectile speed must be positive");
    assert(std::isfinite(proj.m_launch_speed) && "Invalid projectile speed");

    // Continue with logic
}

// Add debug-only checks
#ifndef NDEBUG
    if (!is_valid_input(data)) {
        std::cerr << "Warning: Invalid input detected\n";
    }
#endif
```

---

## Reverse Engineering

### Wrap Structures, Do Not Scatter Offsets

```cpp
class Player final : public omath::rev_eng::ExternalReverseEngineeredObject<MyTrait> {
public:
    using ExternalReverseEngineeredObject::ExternalReverseEngineeredObject;

    [[nodiscard]] omath::Vector3<float> origin() const { return get_by_offset<omath::Vector3<float>>(0x134); }
    [[nodiscard]] int health() const { return get_by_offset<int>(0x140); }
};
```

One class per game structure keeps every offset in one place and gives it a name. See [External object](rev_eng/external_rev_object.md).

### Resolve Addresses with Signatures

Offsets and function addresses move with every game update. Find them with a pattern scan at start-up and fail loudly when the pattern disappears:

```cpp
const auto address = omath::PePatternScanner::scan_for_pattern_in_loaded_module(module_base, "48 8B 05 ?? ?? ?? ??");
if (!address)
    throw std::runtime_error("signature outdated");
```

Keep patterns short but specific, wildcard the bytes that hold addresses and displacements, and re-check a match with a second pattern when the game has several similar functions.

### Treat Remote Memory as Untrusted

Everything read from another process can be stale or torn. Validate pointers and counts before you follow or loop over them, and check `std::isfinite` on floats before feeding them to the camera or the prediction engines.

---

## Memory & Resources

### RAII for Resources

```cpp
// Good: Automatic cleanup
class GameOverlay {
    se::Camera camera_;
    std::vector<Entity> entities_;

public:
    GameOverlay(/* ... */) : camera_(/* ... */) {
        entities_.reserve(1000);
    }

    // Resources cleaned up automatically
    ~GameOverlay() = default;
};
```

### Avoid Unnecessary Copies

```cpp
// Good: Pass by const reference
void draw_entities(const std::vector<omath::Vector3<float>>& positions) {
    for (const auto& pos : positions) {
        // Process position
    }
}

// Bad: Copies entire vector
void draw_entities(std::vector<omath::Vector3<float>> positions) {
    // Expensive copy!
}

// Good: Move when transferring ownership
std::vector<omath::Vector3<float>> compute_positions();
auto positions = compute_positions();  // Move, not copy
```

Camera getters such as `get_view_matrix()` and `get_origin()` return const references. Bind them with `const auto&` instead of copying.

### Decompose with `as_tuple`

```cpp
// Good: Decompose a vector
auto [x, y, z] = position.as_tuple();
```

---

## Documentation

### Document Assumptions

```cpp
// Good: Clear documentation
/**
 * Projects world position to screen space.
 *
 * @param world_pos Position in world coordinates (game units)
 * @return Screen position if visible, the projection error otherwise
 *
 * @note Assumes the camera origin and angles were set this frame
 * @note Screen coordinates are in viewport space [0, width] x [0, height], origin top-left
 */
std::expected<omath::Vector3<float>, omath::projection::Error> project(const omath::Vector3<float>& world_pos);
```

### Explain Non-Obvious Code

```cpp
// Good: Explain the math
// Use squared distance to avoid expensive sqrt
// max_range = 100.0 -> max_range_sq = 10000.0
constexpr float max_range_sq = 100.0f * 100.0f;
if (dist_sq < max_range_sq) {
    // Entity is in range
}

// Explain engine-specific quirks
// Source Engine uses Z-up coordinates, and its pitch is positive downwards,
// while AimAngles from the prediction engines are positive upwards
ViewAngles angles{se::PitchAngle::from_degrees(-aim.angles.pitch), yaw, roll};
```

---

## Debugging

### Add Debug Visualization

```cpp
#ifndef NDEBUG
void debug_draw_projection() {
    // Draw world axes
    draw_line({0,0,0}, {100,0,0}, omath::Color::red());    // X
    draw_line({0,0,0}, {0,100,0}, omath::Color::green());  // Y
    draw_line({0,0,0}, {0,0,100}, omath::Color::blue());   // Z

    // Draw projected points
    for (const auto& entity : entities) {
        if (auto screen = camera.world_to_screen(entity.pos)) {
            draw_cross(screen->x, screen->y);
        }
    }
}
#endif
```

### Log Important Values

```cpp
void debug_projection_failure(const omath::Vector3<float>& pos) {
    std::cerr << std::format("Projection failed for position: {}\n", pos);
    std::cerr << "View matrix:\n" << camera.get_view_matrix().to_string() << "\n";
    std::cerr << std::format("Camera position: {}\n", camera.get_origin());
}
```

### Use Debug Builds

```cmake
# CMakeLists.txt
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(your_target PRIVATE
        DEBUG_PROJECTION=1
        VALIDATE_INPUTS=1
    )
endif()
```

```cpp
#ifdef DEBUG_PROJECTION
    std::cout << "Projecting: " << std::format("{}", world_pos) << "\n";
#endif

#ifdef VALIDATE_INPUTS
    assert(std::isfinite(world_pos.length()));
#endif
```

---

## Platform Considerations

### Cross-Platform Code

```cpp
// Good: Platform-agnostic
constexpr float pi = std::numbers::pi_v<float>;

// Avoid: Platform-specific
#ifdef _WIN32
    // Windows-only code
#endif
```

Keep platform-specific code (process memory access, hooks) behind your own memory trait or interface, so the math stays portable. Pattern scanning of files works for PE, ELF and Mach-O on every host, and `HooksManager` is available on Windows and Linux only.

### Handle Different Compilers

```cpp
// Good: Compiler-agnostic
#if defined(_MSC_VER)
    // MSVC-specific
#elif defined(__GNUC__)
    // GCC/Clang-specific
#endif

// Use OMath's built-in compatibility
// It handles compiler differences automatically
```

On MSVC, linking `omath::omath` defines `NOMINMAX` for you, so `std::min`/`std::max` keep working with the Windows headers. If you include `<Windows.h>` in code that does not link the target, define it yourself.

### Keep Build Options Consistent

`OMATH_USE_AVX2`, `OMATH_IMGUI_INTEGRATION`, `OMATH_ENABLE_HOOKING`, `OMATH_ENABLE_LUA` and a few more are public compile definitions of the `omath` target and change what the headers contain. Link through `omath::omath` so your code is compiled with the same set as the library; if you add include paths by hand, define the same macros.

---

## Summary

**Key principles:**
1. **Safety first**: Always check optional/expected results
2. **Performance matters**: Use constexpr, avoid allocations, cache results
3. **Type safety**: Use strong types, match engine types
4. **Clear code**: Use aliases, document assumptions, explain non-obvious logic
5. **Test thoroughly**: Validate inputs, test edge cases, add assertions
6. **Debug effectively**: Add visualization, log values, use debug builds
7. **Build consistently**: One set of `OMATH_*` options for library and application

---

## See Also

- [Troubleshooting Guide](troubleshooting.md)
- [FAQ](faq.md)
- [API Overview](api_overview.md)
- [Tutorials](tutorials.md)
