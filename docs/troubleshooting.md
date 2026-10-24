# Troubleshooting

Solutions to common problems when using OMath.

---

## Build & Compilation Issues

### Error: C++23 features not available

**Problem:** Compiler doesn't support C++23 (OMath needs `std::expected`, `if consteval`, multidimensional `operator[]` and more).

**Solution:**
Upgrade your compiler to one of the versions the project documents:
- **GCC**: Version 14 or newer
- **Clang**: Version 18 or newer
- **MSVC**: Visual Studio 2022 17.8 (19.38) or newer

Linking the `omath::omath` target already requests C++23. For targets that only add include directories, set it yourself:
```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Error: `std::expected` not found

**Problem:** Building in C++20 mode, or with a standard library that does not ship `<expected>`.

**Solution:**

1. **Use C++23** with a recent standard library:
   ```cmake
   set(CMAKE_CXX_STANDARD 23)
   ```
2. There is no backport mode: OMath's public API returns `std::expected` (`Vector3::angle_between`, `Camera::world_to_screen`, ...), so C++20 builds are not supported.

### Error: `omath/omath.hpp` not found

**Problem:** OMath not installed or not in include path.

**Solution:**

Check installation:
```bash
# vcpkg
vcpkg list | grep omath

# Check if files exist
ls /path/to/vcpkg/installed/x64-linux/include/omath
```

In CMakeLists.txt:
```cmake
find_package(omath CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE omath::omath)
```

### Error: `'X' is not a member of 'omath'` for a header you included

**Problem:** `<omath/omath.hpp>` pulls in most of the library, but not everything.

**Solution:** Include the header explicitly. The ones not covered are: `omath/algorithm/radar.hpp`, `omath/algorithm/targeting.hpp`, `omath/pathfinding/walk_bot.hpp`, `omath/3d_primitives/aabb.hpp`/`obb.hpp`/`mesh.hpp`, `omath/collision/mesh_collider.hpp`, each engine's `mesh.hpp`/`collider.hpp`/`primitives.hpp`, `omath/utility/elf_pattern_scan.hpp`, `omath/utility/macho_pattern_scan.hpp`, `omath/hud/*`, `omath/hooks/hooks_manager.hpp`, `omath/lua/lua.hpp` and `omath/containers/encrypted_variable.hpp`.

### Error: `'ViewAngles': ambiguous symbol`

**Problem:** `omath::ViewAngles` is a class template, and each engine namespace defines an alias with the same name.

```cpp
using namespace omath;
using namespace omath::source_engine;
ViewAngles angles;   // error: ambiguous
```

**Solution:** Do not import both namespaces. Use an alias:
```cpp
namespace se = omath::source_engine;
se::ViewAngles angles;
```

### Error: no matching constructor for an engine `Camera` (Unreal)

**Problem:** The Unreal camera is double precision.

**Solution:** Use `omath::Vector3<double>` positions and `unreal_engine::ViewAngles`. Every other engine uses `float`.

### Error: `HooksManager` has no member `hook_dx11` / `set_on_present` / ...

**Problem:** OMath was built without `OMATH_ENABLE_HOOKING`. In that configuration `HooksManager` is an empty stub with only `get()`.

**Solution:** Rebuild OMath with `-DOMATH_ENABLE_HOOKING=ON` (or the `hooking` vcpkg feature). Hooking is supported on Windows and Linux, not on macOS or ARM. If you do not link through the CMake target, define `OMATH_ENABLE_HOOKING` yourself, the class layout depends on it.

### Error: `ImguiHudRenderer` not declared, or `to_im_vec2` is not a member

**Problem:** OMath was built without `OMATH_IMGUI_INTEGRATION`, so the ImGui conversions and `ImguiHudRenderer` are compiled out.

**Solution:** Enable `OMATH_IMGUI_INTEGRATION` (vcpkg feature `imgui`) and make sure `imgui.h` is on your include path. The macro is a public definition of the `omath` target; define it yourself if you only add include directories.

### Error: `omath::lua::LuaInterpreter` not declared

**Problem:** The Lua bindings are compiled only with `OMATH_ENABLE_LUA`, and `omath/lua/lua.hpp` includes `<sol/forward.hpp>`.

**Solution:** Build OMath with `-DOMATH_ENABLE_LUA=ON` (vcpkg feature `lua`), and make sol2's headers visible to your target.

### Linker errors: undefined reference to `omath::...`

**Problem:** Part of OMath is compiled (pattern scanners, `ProjPredEngineAvx2`, pathfinding, HUD, hooks, Lua), and the library is not linked.

**Solution:**
```cmake
target_link_libraries(your_target PRIVATE omath::omath)
```

### Compiler errors about AVX2 intrinsics ("target specific option mismatch")

**Problem:** `OMATH_USE_AVX2` is defined, but the compiler is not told to generate AVX2 code. This happens when you define the macro by hand instead of linking `omath::omath`, which adds `-mavx2 -mfma` (or `/arch:AVX2`) for you.

**Solution:**
```cmake
if(MSVC)
    target_compile_options(your_target PRIVATE /arch:AVX2)
else()
    target_compile_options(your_target PRIVATE -mavx2 -mfma)
endif()
```
Or build OMath with `-DOMATH_USE_AVX2=OFF`.

### `ProjPredEngineAvx2` throws "AVX2 feature is not enabled!"

**Problem:** OMath was built without `OMATH_USE_AVX2` (or for a non-x86 target), so the AVX2 engine compiles to a stub that throws `std::runtime_error`.

**Solution:** Enable `OMATH_USE_AVX2`, or use the portable engine instead:
```cpp
// Instead of ProjPredEngineAvx2
omath::projectile_prediction::ProjPredEngineLegacy<> engine(800.f, 1.f / 1000.f, 10.f, 5.f);
```

---

## Runtime Issues

### `world_to_screen()` always fails

`world_to_screen()` returns `std::expected<Vector3, projection::Error>`. Print the error first:

| Error | Meaning |
|-------|---------|
| `PERSPECTIVE_DIVIDER_LESS_EQ_ZERO` | The point is behind the camera (or on its plane) |
| `WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS` | The point projects outside the screen. `world_to_screen_unclipped()` keeps such points |
| `INV_VIEW_PROJ_MAT_DET_EQ_ZERO` | Only from `screen_to_world()`: the view-projection matrix cannot be inverted (degenerate camera) |

**Common causes:**

1. **Point behind camera**
   ```cpp
   // Point is behind the camera (Source: forward is +X)
   omath::Vector3<float> behind = camera.get_origin() - omath::Vector3<float>{100, 0, 0};
   auto result = camera.world_to_screen(behind);  // PERSPECTIVE_DIVIDER_LESS_EQ_ZERO
   ```

   **Fix:** Only project points in front of camera. `camera.world_to_view_coordinates(point)` gives the point in view space.

2. **Invalid near/far planes**
   ```cpp
   // Bad: near >= far
   se::Camera camera(pos, angles, viewport, fov, 100.0f, 1.0f);

   // Good: near < far
   se::Camera camera(pos, angles, viewport, fov, 0.1f, 1000.0f);
   ```

3. **Invalid FOV**
   ```cpp
   // Bad: FOV is clamped to [0, 180], 0 is degenerate
   auto fov = omath::projection::FieldOfView::from_degrees(0.0f);

   // Good: a normal value
   auto fov = omath::projection::FieldOfView::from_degrees(90.0f);
   ```

4. **Stale or zero viewport**
   ```cpp
   // ViewPort must be the real screen/window size
   camera.set_view_port({1920.0f, 1080.0f});
   ```

5. **Camera not updated this frame**
   ```cpp
   camera.set_origin(current_position);
   camera.set_view_angles(current_angles);
   ```

**Debugging:**
```cpp
omath::Vector3<float> world_pos{100, 100, 100};

std::cout << "World pos: " << world_pos.x << ", " << world_pos.y << ", " << world_pos.z << "\n";

// Position of the point in camera space: check whether it is in front of the camera
const auto view_space = camera.world_to_view_coordinates(world_pos);
std::cout << "View space: " << view_space.x << ", " << view_space.y << ", " << view_space.z << "\n";

if (auto screen = camera.world_to_screen(world_pos)) {
    std::cout << "Success: " << screen->x << ", " << screen->y << "\n";
} else {
    std::cout << "Failed, error " << static_cast<int>(screen.error()) << "\n";
}
```

### Projection works in one game but is wrong in another

**Problem:** Wrong FOV semantics. Source and IW Engine define their FOV as horizontal at a 4:3 reference aspect ratio, Unreal and Serious Engine as plain horizontal FOV, and Unity, Frostbite, CryEngine, RAGE and OpenGL as vertical FOV. Feed the engine the FOV value in the form the game stores it.

### Angles wrapping incorrectly

**Problem:** Angles not normalizing to expected ranges.

**Solution:**

Use the engine's angle types:
```cpp
namespace se = omath::source_engine;

// Wrong: using raw floats
float pitch = 95.0f;  // Out of valid range!

// Right: using typed angles
auto pitch = se::PitchAngle::from_degrees(95.0f);  // Clamped to 89
```

For custom ranges:
```cpp
// Define custom angle with wrapping
auto angle = omath::Angle<float, -180.0f, 180.0f, omath::AngleFlags::Normalized>::from_degrees(270.0f);
// Result: -90 degrees (wrapped)
```

`AngleFlags::Normalized` wraps around the range, `AngleFlags::Clamped` clamps to its ends.

### Projection appears mirrored or inverted

**Problem:** Using wrong engine trait for your game.

**Solution:**

Different engines have different coordinate systems:

| Engine | Up | Right | Forward |
|--------|----|-------|---------|
| Source, IW Engine | +Z | -Y | +X |
| Unreal | +Z | +Y | +X |
| CryEngine, RAGE | +Z | +X | +Y |
| Unity, Frostbite | +Y | +X | +Z |
| OpenGL, Serious Engine | +Y | +X | -Z |

| Symptom | Likely Issue | Fix |
|---------|-------------|-----|
| Upside down | Wrong up axis | Try the engine with the right up axis |
| Left-right flipped | Wrong handedness | Check the engine's right axis in the table |
| Rotated 90° | Axis swap | Verify engine coordinate system |

```cpp
// Try different engine cameras
omath::source_engine::Camera;    // Z-up
omath::unity_engine::Camera;     // Y-up
omath::unreal_engine::Camera;    // Z-up, double precision
omath::opengl_engine::Camera;    // Y-up, camera looks down -Z
```

If still wrong, manually transform coordinates into the engine's convention before projecting, or write your own [engine trait](faq.md#can-i-extend-omath-with-custom-engine-traits).

The NDC depth range matters when you feed the projection matrix to a graphics API: the OpenGL engine produces -1..1, all others 0..1.

---

## Projectile Prediction Issues

### `maybe_calculate_aim()` returns `nullopt`

**Common causes:**

1. **Target moving too fast**
   ```cpp
   constexpr Target<float> target{.m_origin = {500, 0, 0}, .m_velocity = {1000, 0, 0}};
   constexpr Projectile<float> proj{.m_launch_speed = 500.0f, .m_gravity_scale = 1.0f};

   // Returns nullopt - the round cannot catch a target running away faster than it flies
   ```

   **Fix:** The launch speed must exceed the target's speed along its direction of travel.

2. **Zero or negative step / horizon**
   ```cpp
   // Both must be positive, otherwise the legacy and drag engines return nullopt immediately
   ProjPredEngineLegacy<> engine(800.f, /*step*/ 0.f, /*horizon*/ 10.f, 5.f);
   ```

3. **Invalid positions**
   ```cpp
   // NaN or infinite values
   target.m_origin = Vector3<float>{NAN, 0, 0};
   // No solution
   ```

   **Fix:** Validate all input values are finite.

4. **Target out of range**
   ```cpp
   // The flight must fit inside maximum_simulation_time
   float distance = launcher.eye_origin.distance_to(target.m_origin);
   float rough_flight_time = distance / proj.m_launch_speed;
   if (rough_flight_time > max_flight_time) {
       // No solution
   }
   ```

5. **Pitch outside the engine's limit**: with a `launch_pitch_offset` the solved view pitch can fall outside what the engine lets a player look at (Source: +-89).

**Debugging:**
```cpp
// Check inputs
assert(proj.m_launch_speed > 0);
assert(std::isfinite(target.m_origin.length()));
assert(std::isfinite(target.m_velocity.length()));

float distance = launcher.eye_origin.distance_to(target.m_origin);
float target_speed = target.m_velocity.length();

std::cout << "Distance: " << distance << "\n";
std::cout << "Projectile speed: " << proj.m_launch_speed << "\n";
std::cout << "Target speed: " << target_speed << "\n";

if (target_speed >= proj.m_launch_speed) {
    std::cout << "Target may be too fast!\n";
}
```

### Aim point is inaccurate

**Problem:** Calculated aim doesn't hit target.

**Possible causes:**

1. **Unit mismatch**
   ```cpp
   // All units must match: world units, seconds
   constexpr Projectile<float> proj{.m_launch_speed = 800.0f};   // game units per second
   // target velocity must be in game units per second too, gravity in units/s^2 (Source: 800)
   ```

2. **Wrong gravity constant or scale**
   ```cpp
   // The constant is a magnitude; the engine trait applies it along its own down axis
   ProjPredEngineLegacy<> source_engine_pred(800.f, 1.f / 1000.f, 10.f, 5.f);                          // Source
   ProjPredEngineLegacy<omath::unity_engine::PredEngineTrait> unity_pred(9.81f, 1.f / 1000.f, 10.f, 5.f); // Unity (Y up)
   ```
   `Projectile::m_gravity_scale` multiplies the constant for the round (many weapons use a different scale).

3. **The round has air drag**: the closed-form engines ignore it. Use `ProjPredEngineDrag` and set `Projectile::m_drag`, `m_drag_forward`, `m_drag_up` and `m_max_speed`. Its time step must be the game's physics tick.

4. **Muzzle offset or pitch offset missing**: describe where the round spawns with `Launcher::muzzle_offset` and weapons that fire above the crosshair with `Launcher::launch_pitch_offset`.

5. **Target velocity not updated**
   ```cpp
   // Update target velocity each frame
   target.m_velocity = current_velocity;  // Not last frame's velocity!
   ```

6. **Pitch sign**: `AimAngles::pitch` is positive upwards for every engine, but Source's `PitchAngle` is positive downwards. Source: `se::PitchAngle::from_degrees(-aim.angles.pitch)`.

---

## Pattern Scanning Issues

### Pattern not found when it should be

**Problem:** `scan_for_pattern()` returns the `end()` iterator but the pattern exists.

**Solutions:**

1. **Pattern syntax error**
   ```cpp
   // Wrong: missing spaces. 3+ character tokens are skipped, so the pattern silently gets shorter
   scan_for_pattern(view, "488B05????????");

   // Right: two hex digits per byte, spaces between bytes, ?? or ? for wildcards
   scan_for_pattern(view, "48 8B 05 ?? ?? ?? ??");
   ```
   A token that is not a hex byte or wildcard (such as `X`) makes the whole pattern invalid and the scan returns `end()`.

2. **Pattern too specific**
   ```cpp
   // May fail if any byte is different
   "48 8B 05 01 02 03 04 48 85 C0"

   // Better: use wildcards for variable bytes
   "48 8B 05 ?? ?? ?? ?? 48 85 C0"
   ```

3. **Searching wrong memory region or section**
   ```cpp
   // The PE/ELF/Mach-O scanners only look into one section: .text, .text, __text by default
   const auto result = omath::PePatternScanner::scan_for_pattern_in_file("game.exe", "48 8B 05 ?? ?? ?? ??", ".rdata");
   ```

4. **Pattern might have multiple matches**
   ```cpp
   // scan_for_pattern returns the first match. To find all matches, search again after it
   std::span<std::byte> view{memory};
   auto first = view.begin();
   while (first != view.end()) {
       std::span<std::byte> rest{first, view.end()};
       const auto it = omath::PatternScanner::scan_for_pattern(rest, "48 8B 05 ?? ?? ?? ??");
       if (it == rest.end())
           break;
       std::cout << "Match at: " << (it - view.begin()) << "\n";
       first = it + 1;
   }
   ```

5. **Passing a temporary span**: `scan_for_pattern(std::span<std::byte>{...}, ...)` with an rvalue span does not compile on purpose, because the returned iterator would dangle. Keep the span in a variable.

### Scanning a loaded module crashes or returns nothing

**Problem:** `scan_for_pattern_in_loaded_module()` needs the module's real base address and reads memory directly.

**Solution:** Pass the base returned by `GetModuleHandle` / the loader, make sure the module is fully mapped, and check the section name. A null base, an invalid header or a missing section returns `std::nullopt`.

### Pattern found at wrong location

**Problem:** Pattern matches unintended code.

**Solutions:**

1. **Make pattern more specific**
   ```cpp
   // Too generic
   "48 8B"

   // More specific - include more context
   "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ??"
   ```

2. **Verify found address**
   ```cpp
   if (found != view.end()) {
       const auto offset = found - view.begin();
       // Check if nearby bytes make sense
       if (memory[offset] == std::byte{0x48} && memory[offset + 1] == std::byte{0x8B}) {
           // Looks good
       }
   }
   ```

3. **Use multiple patterns**
   Find a reference function first, then search near that location to get context validation.

---

## Vector & Math Issues

### `normalized()` returns zero vector

**Problem:** Normalizing a zero-length vector.

**Behavior:**
```cpp
omath::Vector3<float> zero{0, 0, 0};
auto result = zero.normalized();  // Returns {0, 0, 0}
```

This is **intentional** to avoid NaN. Check vector length first if you need to know:
```cpp
if (v.length() > 0.001f) {
    auto normalized = v.normalized();
    // Use normalized vector
} else {
    // Handle zero-length case
}
```

### `length()` / `normalized()` do not compile for an integer vector

**Problem:** These functions need a floating point `Type`.

**Solution:** Use `Vector3<float>`/`Vector3<double>`, or `length_sqr()` / `distance_to_sqr()`, which work for every arithmetic type.

### `angle_between()` returns error

**Problem:** One or both vectors have zero length.

**Solution:**
```cpp
auto angle_result = v1.angle_between(v2);

if (angle_result) {
    float degrees = angle_result->as_degrees();
} else {
    // angle_result.error() == Vector3Error::IMPOSSIBLE_BETWEEN_ANGLE
    std::cerr << "Cannot compute angle between zero-length vectors\n";
}
```

### Cross product seems wrong

**Problem:** Unexpected cross product result.

**Check:**
1. **Right-handed formula**
   ```cpp
   omath::Vector3<float> x{1, 0, 0};
   omath::Vector3<float> y{0, 1, 0};
   auto z = x.cross(y);  // {0, 0, 1}
   ```
   `cross()` always uses the standard formula; whether the result points "up" or "down" in your engine depends on the engine's handedness.

2. **Order matters**
   ```cpp
   auto cross1 = a.cross(b);  // {x1, y1, z1}
   auto cross2 = b.cross(a);  // {-x1, -y1, -z1} (opposite direction!)
   ```

### `Mat::inverted()` gives no value

**Problem:** The matrix is singular (determinant is zero), so `inverted()` returns an empty `std::optional`. Check the result before dereferencing.

---

## Performance Issues

### Code is slower than expected

**Solutions:**

1. **Enable optimizations**
   ```cmake
   # CMakeLists.txt
   set(CMAKE_BUILD_TYPE Release)
   ```
   OMath relies on inlining; debug builds are far slower.

2. **Enable AVX2 on x86-64**
   Build OMath with `-DOMATH_USE_AVX2=ON` (the default when the compiler supports it). It speeds up matrix multiplication, pattern scanning and `ProjPredEngineAvx2`.

3. **Avoid unnecessary operations**
   ```cpp
   // Bad: recompute every frame
   for (auto& entity : entities) {
       float dist = entity.pos.distance_to(player_pos);  // Expensive sqrt!
       if (dist < 100.0f) { /* ... */ }
   }

   // Good: use squared distance
   constexpr float max_dist_sq = 100.0f * 100.0f;
   for (auto& entity : entities) {
       float dist_sq = entity.pos.distance_to_sqr(player_pos);  // No sqrt!
       if (dist_sq < max_dist_sq) { /* ... */ }
   }
   ```

4. **Cache matrices**
   ```cpp
   // Bad: constructing a camera for every entity
   for (auto& pos : positions) {
       se::Camera cam(origin, angles, viewport, fov, near_plane, far_plane);
       auto screen = cam.world_to_screen(pos);
   }

   // Good: one camera, matrices are cached and rebuilt only when a setter changes them
   camera.set_origin(origin);
   camera.set_view_angles(angles);
   for (auto& pos : positions) {
       auto screen = camera.world_to_screen(pos);
   }
   ```

5. **Cull before projecting**
   `camera.is_aabb_culled_by_frustum(box)` rejects boxes outside the view before you do any per-vertex work.

6. **Predict less often**
   `ProjPredEngineLegacy` scans in time steps of `simulation_time_step`; a 1 ms step over a 10 s horizon is up to 10,000 iterations. Use a coarser step or a shorter horizon if the target is always close.

---

## Getting More Help

If your issue isn't covered here:

1. **Check the docs**: [API Overview](api_overview.md), [Tutorials](tutorials.md), [FAQ](faq.md)
2. **Search GitHub issues**: [Issues page](https://github.com/orange-cpp/omath/issues)
3. **Ask on Discord**: [Join community](https://discord.gg/eDgdaWbqwZ)
4. **Open a new issue**: Include:
   - OMath version
   - Compiler and version
   - Minimal reproducible example
   - What you expected vs what happened
