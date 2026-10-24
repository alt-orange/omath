# FAQ

Common questions and answers about OMath.

---

## General Questions

### What is OMath?

OMath is a modern C++23 math library designed for game development, graphics programming, and high-performance computing. It provides:
- Vector, matrix and quaternion operations
- 3D projection and camera systems
- Projectile prediction (including air drag)
- Collision detection (ray tracing, GJK/EPA, AABB/OBB)
- Pathfinding and a walk bot
- Support for multiple game engines (Source, Unity, Unreal, Frostbite, IW Engine, CryEngine, RAGE, Serious Engine, OpenGL)
- A HUD/ESP overlay toolkit, DirectX/OpenGL hooking and Lua bindings (opt-in)
- Reverse engineering helpers and pattern scanning for PE, ELF and Mach-O

### Why choose OMath over other math libraries?

- **Modern C++**: Uses C++23 features (concepts, `constexpr`, `std::expected`, `if consteval`, multidimensional `operator[]`)
- **No legacy code**: Built from scratch without legacy baggage
- **Game engine support**: Pre-configured for nine engine conventions
- **Few dependencies**: The core needs nothing; ImGui, Lua and safetyhook are required only by the optional features
- **Performance**: AVX2 optimizations available
- **Type safety**: Strong typing prevents common errors
- **Cross-platform**: Works on Windows, Linux, and macOS

### Is OMath suitable for production use?

Yes! OMath is used in various projects. It has:
- A large unit test suite
- Clear error handling
- A documented API
- Active maintenance and community support

---

## Installation & Setup

### How do I install OMath?

Four main methods:

**vcpkg (recommended):**
```bash
vcpkg install orange-math
```

**xrepo:**
```bash
xrepo install omath
```

**Conan:**
```bash
conan install --requires="omath/[*]" --build=missing
```

**From source:**
See [Installation Guide](install.md)

### What are the minimum requirements?

- **Compiler**: C++23 support required. The project documents:
  - GCC 14+
  - Clang 18+
  - MSVC 19.38+ (Visual Studio 2022 17.8 or newer)
- **CMake**: 3.26+ (if building from source), 3.28+ for the C++ module
- **Platform**: Windows, Linux, or macOS (the hooks: Windows and Linux)

### Do I need C++23?

Yes. The `omath` target requires `cxx_std_23`. The headers use `std::expected`, `if consteval`, multidimensional `operator[]` and other C++23 features, so there is no C++20 fallback.

### Can I use OMath in a C++17 or C++20 project?

No, OMath requires C++23.

### How do I turn on the HUD ImGui renderer, hooks or Lua bindings?

They are build options: `OMATH_IMGUI_INTEGRATION`, `OMATH_ENABLE_HOOKING` and `OMATH_ENABLE_LUA` (vcpkg features `imgui`, `hooking`, `lua`). See [Optional features](install.md#optional-features).

### Can I use OMath as a C++ module?

Yes, build with `OMATH_ENABLE_MODULES=ON` (CMake 3.28+) and write `import omath;`.

---

## Usage Questions

### How do I include OMath in my project?

**Most of the library:**
```cpp
#include <omath/omath.hpp>
```

**Specific components:**
```cpp
#include <omath/linear_algebra/vector3.hpp>
#include <omath/engines/source_engine/camera.hpp>
```

`omath.hpp` does not include everything. Include these explicitly when you need them: `omath/algorithm/radar.hpp`, `omath/algorithm/targeting.hpp`, `omath/pathfinding/walk_bot.hpp`, `omath/3d_primitives/aabb.hpp`/`obb.hpp`/`mesh.hpp`, the per-engine `mesh.hpp`/`collider.hpp`/`primitives.hpp`, `omath/collision/mesh_collider.hpp`, `omath/utility/elf_pattern_scan.hpp`, `omath/utility/macho_pattern_scan.hpp`, `omath/hud/*`, `omath/hooks/hooks_manager.hpp`, `omath/lua/lua.hpp` and `omath/containers/encrypted_variable.hpp`.

### Which game engine should I use?

Choose based on your target game or application:

| Engine | Namespace | Use For |
|--------|-----------|---------|
| **Source Engine** | `omath::source_engine` | CS:GO, TF2, CS2, Half-Life, Portal, L4D |
| **Unity Engine** | `omath::unity_engine` | Unity games (many indie and mobile games) |
| **Unreal Engine** | `omath::unreal_engine` | Fortnite, Unreal games (double precision) |
| **Frostbite** | `omath::frostbite_engine` | Battlefield, Star Wars games (EA titles) |
| **IW Engine** | `omath::iw_engine` | Call of Duty series |
| **CryEngine** | `omath::cry_engine` | CryEngine titles |
| **RAGE** | `omath::rage_engine` | Rockstar's RAGE titles such as GTA V |
| **Serious Engine** | `omath::serious_engine` | Serious Sam and other Serious Engine titles |
| **OpenGL** | `omath::opengl_engine` | Custom OpenGL applications, generic 3D |

### How do I switch between engines?

Change the namespace, ideally through an alias so there is one line to edit:

```cpp
// Source Engine
namespace eng = omath::source_engine;
eng::Camera cam = /* ... */;

// Unity Engine
namespace eng = omath::unity_engine;
eng::Camera cam = /* ... */;
```

Each engine has the same shape of API (`Camera`, `ViewAngles`, `PitchAngle`, `Mat4X4`, `CameraTrait`, `PredEngineTrait`, ...) but different coordinate system handling. The types are not interchangeable: a Unity camera takes Unity `ViewAngles`, and Unreal uses `double` throughout.

### Why is `ViewAngles` ambiguous?

`omath::ViewAngles` is a class template and every engine namespace defines an alias called `ViewAngles`. `using namespace omath;` together with `using namespace omath::source_engine;` therefore makes the name ambiguous. Use a namespace alias (`namespace se = omath::source_engine;`) or qualify the name.

### What if my game isn't listed?

Use the **OpenGL engine** as a starting point - it uses canonical OpenGL conventions. You may need to adjust coordinate transformations based on your specific game, or write your own engine trait (see [below](#can-i-extend-omath-with-custom-engine-traits)).

---

## Performance Questions

### Should I use the AVX2 or Legacy engine?

**Use `ProjPredEngineAvx2` if:**
- You target x86-64 and built OMath with `OMATH_USE_AVX2`
- Your game uses Source-style axes (z up, yaw about z) and `float`

**Use `ProjPredEngineLegacy` if:**
- You need broad compatibility (ARM, builds without AVX2)
- You use another engine trait or `double`
- Unsure about target hardware

Both implement `ProjPredEngineInterface`, so the call is the same, but the constructors differ:
```cpp
using namespace omath::projectile_prediction;

// Legacy (portable, any engine trait)
ProjPredEngineLegacy<> legacy(/*gravity*/ 800.f, /*step*/ 1.f / 1000.f, /*horizon*/ 10.f, /*tolerance*/ 5.f);

// AVX2 (float, Source-style axes; every call throws if the build has no AVX2)
ProjPredEngineAvx2 fast(/*gravity*/ 800.f, /*step*/ 1.f / 1000.f, /*horizon*/ 10.f);

// Drag (rounds with air drag, step should be the game's physics tick)
ProjPredEngineDrag<> drag(/*gravity*/ 800.f, /*step*/ 0.015f, /*horizon*/ 10.f, /*tolerance*/ 5.f);
```

### Which prediction engine handles air drag?

`ProjPredEngineDrag`. Set `Projectile::m_drag` (or `m_drag_forward`/`m_drag_up`) and `m_max_speed`. The closed-form engines assume a parabola and ignore drag. See [Drag engine](projectile_prediction/proj_pred_engine_drag.md).

### Are vector operations constexpr?

Yes! Most operations are `constexpr` and can be evaluated at compile-time:

```cpp
constexpr omath::Vector3<float> v{1, 2, 3};
constexpr auto len_sq = v.length_sqr();  // Computed at compile time
```

### Is OMath thread-safe?

- **Value types** (vectors, matrices, angles) have no shared state
- **Cameras** cache their matrices lazily, so a camera must not be shared between threads, even for read-only calls
- Use separate instances per thread or synchronize access

---

## Troubleshooting

### `world_to_screen()` always fails

`world_to_screen()` returns `std::expected<Vector3, projection::Error>`; look at `.error()`:
- `PERSPECTIVE_DIVIDER_LESS_EQ_ZERO`: the point is behind the camera
- `WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS`: the point projects outside the screen (use `world_to_screen_unclipped()` if you still want it)

If every point fails, check:
1. **Are near/far planes correct?** Ensure `near < far` and both are positive.
2. **Is FOV valid?** It is clamped to [0, 180] degrees; use a normal value such as 60-120.
3. **Are camera angles in the engine's range?** Use the engine-provided angle types.
4. **Is the viewport non-zero?** `ViewPort{width, height}` must not be `{0, 0}`.
5. **Right engine?** The wrong engine trait puts points behind the camera.

### Angles are wrapping incorrectly

Use the correct angle type:
```cpp
// Good: uses the engine's angle type (Source: pitch clamped to +-89)
auto pitch = omath::source_engine::PitchAngle::from_degrees(45.0f);

// Bad: raw float loses normalization
float pitch = 45.0f;
```

### Projection seems mirrored or inverted

You may be using the wrong engine trait. Each engine has different coordinate conventions:

| Engine | Up | Right | Forward |
|--------|----|-------|---------|
| Source, IW Engine | +Z | -Y | +X |
| Unreal | +Z | +Y | +X |
| CryEngine, RAGE | +Z | +X | +Y |
| Unity, Frostbite | +Y | +X | +Z |
| OpenGL, Serious Engine | +Y | +X | -Z |

Ensure you're using the trait matching your game.

### Pattern scanning finds multiple matches

This is normal! Patterns may appear multiple times, and `scan_for_pattern` returns the first one. Solutions:
1. Make the pattern more specific (more bytes, fewer wildcards)
2. Use additional context (nearby code patterns)
3. Verify each match programmatically

### My pattern is not found

Check the pattern string: every byte is exactly two hex digits (`4A`), wildcards are `?` or `??`, and tokens are separated by spaces. A token with three or more characters (for example `488B05`) is silently skipped, which shortens the pattern. `scan_for_pattern` returns `end` for "not found" and for an invalid pattern. The compile-time overload `scan_for_pattern<"48 8B ??">(...)` rejects bad patterns at compile time.

### Projectile prediction returns `nullopt`

Common reasons:
1. **Target too fast**: The round cannot catch the target
2. **Out of range**: The flight would take longer than `maximum_simulation_time`
3. **Invalid input**: A non-positive time step or horizon returns `nullopt` immediately; a launch speed of zero never reaches anything
4. **Pitch limit**: The needed pitch is outside what the engine lets the view look at
5. **`ProjPredEngineAvx2` in a non-AVX2 build**: it throws instead of returning `nullopt`

### `HooksManager` has no `hook_dx11` (or any other hook method)

The library was built without `OMATH_ENABLE_HOOKING`. In that case `HooksManager` is an empty stub that only offers `get()`. Rebuild with the option (or the `hooking` vcpkg feature).

### Compilation errors about `std::expected`

OMath requires C++23. Make sure `CMAKE_CXX_STANDARD` is at least 23 (linking `omath::omath` already requires `cxx_std_23`) and that your standard library ships `<expected>` (GCC 12+, Clang 16+ with libstdc++ 12 or libc++, MSVC 19.33+).

---

## Feature Questions

### Can I use OMath with DirectX/OpenGL/Vulkan?

Yes! OMath produces view and projection matrices and projects points for any graphics API. Pick the engine whose conventions match your renderer, mind the NDC depth range (the OpenGL engine uses -1..1, all other engines 0..1), and use:
- **OpenGL**: `opengl_engine` traits
- **DirectX**: Use the appropriate engine trait or OpenGL as base
- **Vulkan**: Use OpenGL traits as starting point

`HooksManager` can hook DirectX 9/11/12 and OpenGL for you. Vulkan is not hooked.

### Can I draw an ESP overlay with OMath?

Yes. The [HUD toolkit](hud/overview.md) describes boxes, bars, labels, skeletons, crosshairs and more as widget structs, and `ImguiHudRenderer` draws them through ImGui (`OMATH_IMGUI_INTEGRATION`). To draw inside another process combine it with [HooksManager](hooks/hooks_manager.md); the `examples/` directory has complete DLLs.

### Can I script OMath from Lua?

Yes, build with `OMATH_ENABLE_LUA` and call `omath::lua::LuaInterpreter::register_lib(L)`. See [Lua scripting](lua/lua.md).

### Does OMath support quaternions?

Yes. `omath::Quaternion<T>` supports `from_axis_angle()`, multiplication, `rotate()` and conversion to 3x3/4x4 rotation matrices. Cameras still use Euler `ViewAngles`. See [Quaternion](linear_algebra/quaternion.md).

### Can I scan ELF and Mach-O binaries?

Yes. `ElfPatternScanner` and `MachOPatternScanner` mirror `PePatternScanner`: files, in-memory file images and loaded modules. See [ELF](utility/elf_pattern_scan.md) and [Mach-O](utility/macho_pattern_scan.md).

### Can I extend OMath with custom engine traits?

Yes! A camera trait only has to satisfy `CameraEngineConcept`: three `static constexpr noexcept` functions that return exactly your matrix and view-angle types.

```cpp
#include <omath/projection/camera.hpp>

struct MyEngineTrait final {
    static constexpr MyViewAngles
    calc_look_at_angle(const omath::Vector3<float>& cam_origin, const omath::Vector3<float>& look_at) noexcept;

    static constexpr MyMat4X4
    calc_view_matrix(const MyViewAngles& angles, const omath::Vector3<float>& cam_origin) noexcept;

    static constexpr MyMat4X4
    calc_projection_matrix(const omath::projection::FieldOfView& fov, const omath::projection::ViewPort& view_port,
                           float near_plane, float far_plane, omath::NDCDepthRange ndc_depth_range) noexcept;
};

// Use with Camera
using MyCamera = omath::projection::Camera<MyMat4X4, MyViewAngles, MyEngineTrait, omath::NDCDepthRange::ZERO_TO_ONE>;
```

`MyMat4X4` is an `omath::Mat<4, 4, float, ...>` and `MyViewAngles` an `omath::ViewAngles<Pitch, Yaw, Roll>` of your own angle types. Optional `extract_rotation_angles()` and `calc_field_of_view()` let the camera recover data from matrices. For projectile prediction implement the `PredEngineConcept` ([details](projectile_prediction/pred_engine_concept.md)). The built-in traits, for example the [Source camera trait](engines/source_engine/camera_trait.md), are good templates.

### Does OMath support SIMD for vector operations?

AVX2 (`OMATH_USE_AVX2`) is used by the matrix multiply, the pattern scanner and the AVX2 projectile engine. The vector types are plain `constexpr` code that compilers vectorize themselves.

### Can I use OMath for machine learning?

OMath is optimized for game development and graphics, not ML. For machine learning, consider libraries like Eigen or xtensor which are designed for that domain.

---

## Debugging Questions

### How do I print vectors?

OMath provides `std::formatter` support for vectors, quaternions, angles and colors:

```cpp
#include <format>
#include <iostream>

omath::Vector3<float> v{1, 2, 3};
std::cout << std::format("{}", v) << "\n";  // Prints: [1, 2, 3]
```

Matrices have `to_string()` (and a `std::formatter` too).

### How do I visualize projection problems?

1. Check if `world_to_screen()` succeeds and which `Error` it gives
2. Print camera matrices:
   ```cpp
   std::cout << camera.get_view_matrix().to_string() << "\n";
   std::cout << camera.get_projection_matrix().to_string() << "\n";
   ```
3. Test with known good points (e.g., a point straight ahead of the camera)
4. Verify viewport and FOV values

### How can I debug pattern scanning?

```cpp
std::span<std::byte> view{memory};

// Does the pattern exist at all?
const auto it = omath::PatternScanner::scan_for_pattern(view, "48 8B 05 ?? ?? ?? ??");
if (it == view.end()) {
    std::cout << "not found (or the pattern string is invalid)\n";
} else {
    std::cout << "offset " << (it - view.begin()) << "\n";
}

// Narrow the pattern down until it matches, then widen it again
```

---

## Contributing

### How can I contribute to OMath?

See [CONTRIBUTING.md](https://github.com/orange-cpp/omath/blob/master/CONTRIBUTING.md) for guidelines. Contributions welcome:
- Bug fixes
- New features
- Documentation improvements
- Test coverage
- Examples

### Where do I report bugs?

[GitHub Issues](https://github.com/orange-cpp/omath/issues)

Please include:
- OMath version
- Compiler and version
- Minimal reproducible example
- Expected vs actual behavior

### How do I request a feature?

Open a GitHub issue with:
- Use case description
- Proposed API (if applicable)
- Why existing features don't meet your needs

---

## License & Legal

### What license does OMath use?

OMath uses a custom "libomath" license. See [LICENSE](https://github.com/orange-cpp/omath/blob/master/LICENSE) for full details.

### Can I use OMath in commercial projects?

Check the LICENSE file for commercial use terms.

### Can I use OMath for game cheating/hacking?

OMath is a math library and can be used for various purposes. However:
- Using it to cheat in online games may violate game ToS
- Creating cheats may be illegal in your jurisdiction
- The developers do not condone cheating in online games

Use responsibly and ethically.

---

## Getting Help

### Where can I get help?

- **Documentation**: [http://libomath.org](http://libomath.org)
- **Discord**: [Join community](https://discord.gg/eDgdaWbqwZ)
- **Telegram**: [@orangennotes](https://t.me/orangennotes)
- **GitHub Issues**: [Report bugs/ask questions](https://github.com/orange-cpp/omath/issues)

### Is there a Discord/community?

Yes! Join our Discord: [https://discord.gg/eDgdaWbqwZ](https://discord.gg/eDgdaWbqwZ)

### Are there video tutorials?

Check our [YouTube channel](https://youtu.be/lM_NJ1yCunw?si=-Qf5yzDcWbaxAXGQ) for demonstrations and tutorials.

---

## Didn't find your answer?

- Search the [documentation](index.md)
- Check [tutorials](tutorials.md)
- Ask on [Discord](https://discord.gg/eDgdaWbqwZ)
- Open a [GitHub issue](https://github.com/orange-cpp/omath/issues)
