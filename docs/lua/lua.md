# `omath::lua::LuaInterpreter` — Lua bindings for omath

> Header: `omath/lua/lua.hpp`
> Namespace: `omath::lua`
> Sources: `source/lua/*.cpp` (one file per bound area)
> Purpose: expose omath's vectors, matrices, colours, collision, pattern scanning, HUD and per-engine cameras to a Lua state under a global `omath` table
> Requires: `OMATH_ENABLE_LUA` (CMake) / `lua` (vcpkg feature), which brings in Lua and [sol2](https://github.com/ThePhD/sol2)

---

## Requirements

| Item | Details |
|------|---------|
| CMake option | `-DOMATH_ENABLE_LUA=ON` (default `OFF`). Defines `OMATH_ENABLE_LUA` publicly on the `omath` target, runs `find_package(Lua REQUIRED)` and locates sol2 with `find_path(SOL2_INCLUDE_DIRS "sol/abort.hpp")`. Lua and sol2 are linked/included privately |
| vcpkg feature | `lua` (`omath[lua]`), also part of `all`. Depends on the `lua` and `sol2` ports |
| Header | `omath/lua/lua.hpp` includes `<sol/forward.hpp>`, so code that includes it needs sol2's headers on its include path |

Everything in `lua.hpp` and `source/lua/` is compiled only when `OMATH_ENABLE_LUA` is defined; with the option off the header is empty.

---

## API

```cpp
namespace omath::lua {

class LuaInterpreter final {
public:
  static void register_lib(lua_State* lua_state);
  // plus private register_* helpers called by register_lib
};

}
```

There is a single entry point. `register_lib` wraps the state in `sol::state_view`, gets or creates the global table `omath`, and calls the private registration helpers in this order: `register_vec2`, `register_vec3`, `register_vec4`, `register_matrices`, `register_quaternion`, `register_color`, `register_hud`, `register_triangle`, `register_3d_primitives`, `register_collision`, `register_shared_types`, `register_engines`, `register_pattern_scan`.

---

## Registering the bindings

```cpp
#include <lua.hpp>               // Lua's own header
#include <omath/lua/lua.hpp>

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    omath::lua::LuaInterpreter::register_lib(L);   // creates the global `omath` table

    if (luaL_dostring(L, R"(
        local v = omath.Vec3.new(1, 2, 3)
        print(v:length(), v:normalized())
    )") != LUA_OK)
        std::fprintf(stderr, "%s\n", lua_tostring(L, -1));

    lua_close(L);
}
```

If you already use sol2, pass `state.lua_state()` to `register_lib`. The tests in `tests/lua/` load a `.lua` file after registering and call its global test functions.

---

## Using it from Lua

Types are constructed with `.new(...)`, methods are called with `:`, and fallible operations return `value, error_string` pairs instead of raising.

```lua
-- Source-engine camera, world -> screen
local angles = omath.source.ViewAngles.new(
    omath.source.PitchAngle.from_degrees(0),
    omath.source.YawAngle.from_degrees(90),
    omath.source.RollAngle.from_degrees(0))

local cam = omath.source.Camera.new(
    omath.Vec3.new(0, 0, 64),
    angles,
    omath.source.ViewPort.new(1920, 1080),
    omath.source.FieldOfView.from_degrees(90),
    0.1, 1000)

local screen, err = cam:world_to_screen(omath.Vec3.new(0, 500, 64))
if screen then
    print(screen.x, screen.y)
else
    print("not visible: " .. err)
end
```

```lua
-- Draw an entity box through a Lua renderer (callbacks are optional; missing ones are no-ops)
local renderer = omath.hud.Renderer.new({
    add_line       = function(a, b, color, thickness) my_draw_line(a.x, a.y, b.x, b.y, color, thickness) end,
    add_polyline   = function(points, color, thickness) --[[ points is a 1-based table of Vec2 ]] end,
    add_text       = function(pos, color, text) my_draw_text(pos.x, pos.y, text) end,
    calc_text_size = function(text) return omath.Vec2.new(#text * 6, 10) end,
})

local overlay = omath.hud.EntityOverlay.new(omath.Vec2.new(400, 100), omath.Vec2.new(400, 300), renderer)
overlay:add_cornered_2d_box(omath.Color.red())
overlay:add_right_label(omath.Color.green(), 2, true, "Health: 100")
```

---

## Exposed types

All names are fields of the global `omath` table unless a sub-table is shown.

### Linear algebra and utility

| Lua name | C++ type | Highlights |
|----------|----------|-----------|
| `Vec2` | `Vector2<float>` | `new()`, `new(x, y)`; `x`, `y`; `+ - * /`, unary `-`, `==`, `<`, `<=`, `tostring`; `length`, `length_sqr`, `normalized`, `dot`, `distance_to`, `distance_to_sqr`, `sum`, `abs` |
| `Vec3` | `Vector3<float>` | Everything `Vec2` has (plus component-wise `*` and `/` by a `Vec3`) and `z`; `length_2d`, `cross`, `sum_2d`, `point_to_same_direction`, `as_array`, `as_table`, `is_perpendicular(other [, eps])`, `angle_between(other)` returning `degrees` or `nil, error` |
| `Vec4` | `Vector4<float>` | `x`, `y`, `z`, `w`; arithmetic and comparison; `length`, `length_sqr`, `dot`, `sum`, `abs`, `clamp(min, max)` |
| `Mat3` | `Mat<3,3,float>` row-major | `new()`, `from_rows(rows)`, `row_count`, `columns_count`, `at(row, col)` and `set_at(row, col, v)` with **0-based** indices (out of range raises an error), `*` (matrix, scalar, scalar on left), `/` scalar, `==`, `tostring`, `transposed`, `determinant`, `sum`, `clear`, `set(v)`, `inverted()` (`nil` if singular), `as_table` |
| `Mat4` | `Mat<4,4,float>` row-major | As `Mat3`, plus the static helpers `to_screen_mat(w, h)`, `translation(vec3)`, `scale(vec3)`, `look_at_left_handed`, `look_at_right_handed`, `perspective_left_handed_vertical_fov`, `perspective_right_handed_vertical_fov` |
| `Mat4ColumnMajor` | `Mat<4,4,float>` column-major | Same as `Mat4`, column-major storage (the type returned by the OpenGL camera) |
| `Mat4d` | `Mat<4,4,double>` row-major | As `Mat3` without the static helpers (the type of the Unreal camera's matrices) |
| `Quaternion` | `Quaternion<float>` | `new()`, `new(x, y, z, w)`, `from_axis_angle(axis, radians)`; `x y z w`; `+`, `*` (quaternion, scalar), unary `-`, `==`, `tostring`; `conjugate`, `dot`, `length`, `length_sqr`, `normalized`, `inverse`, `rotate(vec3)`, `to_rotation_matrix3`, `to_rotation_matrix4`, `as_table` |
| `Triangle` | `Triangle<Vector3<float>>` | `new()`, `new(v1, v2, v3)`; `vertex1/2/3`; `calculate_normal`, `side_a_length`, `side_b_length`, `side_a_vector`, `side_b_vector`, `hypot`, `is_rectangular`, `mid_point`, `tostring` |
| `Color` | `Color` | `new()`, `new(r, g, b, a)` (channels clamped to `[0, 1]`), `from_rgba(r, g, b, a)` (0-255), `from_hsv(h, s, v)` or `from_hsv(hsv)`, `red()`, `green()`, `blue()`; read-only `r g b a`; `to_hsv`, `set_hue`, `set_saturation`, `set_value`, `blend`, `tostring` |
| `Hsv` | `Hsv` | `new()`; `hue`, `saturation`, `value` |

Matrix construction from rows takes a table of row tables: `omath.Mat4.from_rows({{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}})`.

### Collision and 3D primitives

| Lua name | C++ type | Highlights |
|----------|----------|-----------|
| `primitives.UpAxis` | `primitives::UpAxis` | Enum: `X`, `Y`, `Z` |
| `primitives.Aabb` | `Aabb<float>` | `new()`, `new(min, max)`; `min`, `max`; `center`, `extents`, `top([axis])`, `bottom([axis])` (axis defaults to `Y`), `is_collide(other)`, `as_table` |
| `primitives.Obb` | `Obb<float>` | `new()`, `new(center, axis_x, axis_y, axis_z, half_extents)`; the same five fields; `vertices()` returns a 1-based table of eight `Vec3` |
| `collision.Ray` | `collision::Ray<Vec3>` | `new()`, `new(start, end)`, `new(start, end, infinite_length)`; `start`, `end`, `infinite_length`; `direction_vector`, `direction_vector_normalized` |
| `collision.ConvexCollider` | internal `ColliderInterface<Vec3>` | `new(vertices [, origin])` from a 1-based table of `Vec3`; `find_abs_furthest_vertex_position`, `get_origin`, `set_origin`, `vertex_count`, `vertices()` |
| `collision.LineTracer` | `LineTracer<Ray<Vec3>>` | Static: `can_trace_line`, `get_ray_hit_point(ray, triangle \| aabb \| obb)`, `ray_hits_triangle`, `ray_hits_aabb`, `ray_hits_obb` |
| `collision.gjk_support_vertex(a, b, direction)` | `GjkAlgorithm` | Function; support vertex of the Minkowski difference of two `ConvexCollider`s |
| `collision.gjk_collide(a, b)` | `GjkAlgorithm` | Function; `true` when the colliders overlap |
| `collision.epa_solve(a, b)` | `Epa` | Function; returns `nil` if no collision, otherwise a table with `normal`, `penetration_vector`, `depth`, `iterations`, `num_vertices`, `num_faces` |

### Pattern scanning

| Lua name | Functions |
|----------|-----------|
| `SectionScanResult` | Read-only `virtual_base_addr`, `raw_base_addr`, `target_offset`; `tostring`. No constructor |
| `PatternScanner` | `scan(data, pattern)`: scans a Lua string used as a byte buffer, returns the offset or `nil` |
| `PePatternScanner` | `scan_in_module(base_addr, pattern [, section])` returns an address or `nil`; `scan_in_file(path, pattern [, section])` returns a `SectionScanResult` or `nil`. Default section `.text` |
| `ElfPatternScanner` | Same functions as `PePatternScanner`; default section `.text` |
| `MachOPatternScanner` | Same functions as `PePatternScanner`; default section `__text` |

Patterns use omath's usual syntax (hex bytes separated by spaces, `?` / `??` wildcards). See [Pattern scan](../utility/pattern_scan.md) and [PE pattern scan](../utility/pe_pattern_scan.md). `scan_in_module` interprets `base_addr` as a raw address in the current process.

### HUD

| Lua name | Description |
|----------|-------------|
| `hud.CanvasBox` | `new(top, bottom [, ratio])`; `top_left_corner`, `top_right_corner`, `bottom_left_corner`, `bottom_right_corner`; `as_table()` returns the four corners (1-based) |
| `hud.Renderer` | `new(callbacks)`: a renderer built from a Lua table of callbacks named like the [`HudRendererInterface`](../hud/renderer_interface.md) methods (`add_line`, `add_polyline`, `add_filled_polyline`, `add_rectangle`, `add_filled_rectangle`, `add_circle`, `add_filled_circle`, `add_arc`, `add_image`, `add_text`, `calc_text_size`). Missing callbacks are skipped; a missing `calc_text_size` yields a zero size. The same methods can also be called on the renderer from Lua |
| `hud.EntityOverlay` | `new(top, bottom, renderer)` (box aspect fixed at `4`; a `nil` renderer raises an error). Methods below |
| `hud.ProjectileAimFigure` | Enum: `CIRCLE`, `SQUARE` |

`EntityOverlay` methods (all return the overlay for chaining, optional arguments in brackets): `add_2d_box(color [, fill, outline, thickness])`, `add_cornered_2d_box(color [, fill, outline, corner_ratio, thickness])`, `add_dashed_box(color [, dash_len, gap_len, thickness])`, `add_{right,left,top,bottom}_bar(color, outline, bg, size, ratio [, offset])`, `add_{right,left,top,bottom}_dashed_bar(color, outline, bg, size, ratio, dash_len, gap_len [, offset])`, `add_{right,left,top,bottom}_label(color, offset, outlined, text)`, `add_centered_top_label`, `add_centered_bottom_label`, `add_{right,left,top,bottom}_space_{vertical,horizontal}(size)`, `add_{right,left,top,bottom}_progress_ring(color, bg, radius, ratio [, thickness, offset, segments])`, `add_{right,left,top,bottom}_icon(texture, width, height [, tint, offset])`, `add_snap_line(start, color, width)`, `add_skeleton(color [, thickness])`, `add_scan_marker(color [, outline, outline_thickness])`, `add_aim_dot(position, color [, radius])`, `add_projectile_aim(position, color [, size, line_size, figure])`.

Differences from the C++ API: bar colours are plain `Color`s (no gradients or glow), `outlined` is a boolean, icons take any Lua value as the texture handle, and there is no `ScreenOverlay`, `contents()` widget composition, `Gradient`, `Glow` or `ImguiHudRenderer`. `from_aabb` and custom skeleton bones are not exposed either. For the C++ side see the [HUD overview](../hud/overview.md).

### Engines

`register_engines` creates one sub-table per supported engine and fills each with the same set of fields:

| Sub-table | Engine | Camera class | Pitch range |
|-----------|--------|--------------|-------------|
| `omath.opengl` | OpenGL | `opengl_engine::Camera` | ±90° |
| `omath.frostbite` | Frostbite | `frostbite_engine::Camera` | ±90° |
| `omath.iw` | IW (Call of Duty) | `iw_engine::Camera` | ±89° |
| `omath.source` | Source | `source_engine::Camera` | ±89° |
| `omath.rage` | RAGE | `rage_engine::Camera` | ±90° |
| `omath.serious` | Serious Engine | `serious_engine::Camera` | ±90° |
| `omath.unity` | Unity | `unity_engine::Camera` | ±90° |
| `omath.unreal` | Unreal | `unreal_engine::Camera` (double precision) | see note |
| `omath.cry` | CryEngine | `cry_engine::Camera` | ±90° |

Each sub-table contains:

| Field | Meaning |
|-------|---------|
| `PitchAngle`, `YawAngle`, `RollAngle`, `FieldOfView`, `ViewPort` | Aliases of the shared types below |
| `ViewAngles` | `new(pitch, yaw, roll)`; `pitch`, `yaw`, `roll` properties |
| `Camera` | `new(origin, view_angles, view_port, field_of_view, near, far)` and the methods in the next table |

Shared angle and view types are also available directly in `omath._types`:

| `omath._types` name | C++ type | Behaviour |
|---------------------|----------|-----------|
| `PitchAngle90`, `PitchAngle89` | `Angle<float, ±90 / ±89, Clamped>` | Values outside the range are clamped |
| `YawRoll` | `Angle<float, -180, 180, Normalized>` | Wraps into the range (`200°` becomes `-160°`) |
| `FieldOfView` | `Angle<float, 0, 180, Clamped>` | Clamped to `0..180` |
| `ViewPort` | `projection::ViewPort` | `new(width, height)`; `width`, `height`; `aspect_ratio()` |
| `ViewAngles90`, `ViewAngles89` | `ViewAngles<Pitch, YawRoll, YawRoll>` | `new(pitch, yaw, roll)` |

Every angle type has no constructor; use the statics `from_degrees(v)` and `from_radians(v)`, and the methods `as_degrees`, `as_radians`, `sin`, `cos`, `tan`, `cot`, angle `+` / `-`, unary `-`, `==` and `tostring` (`"45deg"`).

`Camera` methods:

| Group | Methods |
|-------|---------|
| Getters | `get_origin`, `get_view_angles`, `get_near_plane`, `get_far_plane`, `get_field_of_view`, `get_forward`, `get_right`, `get_up` |
| Setters | `set_origin`, `set_view_angles`, `set_view_port`, `set_field_of_view`, `set_near_plane`, `set_far_plane`, `look_at(target)` |
| Matrices | `get_view_matrix`, `get_projection_matrix`, `get_view_projection_matrix` (returned as `Mat4`, `Mat4ColumnMajor` for OpenGL, `Mat4d` for Unreal) |
| Extraction | `extract_projection_params(projection_matrix)` returns `fov, aspect_ratio`; `calc_view_angles_from_view_matrix`, `calc_origin_from_view_matrix` |
| Projection | `world_to_screen(vec3)` and `screen_to_world(vec3)` return `Vec3, nil` on success or `nil, message` on failure (out of screen bounds, singular matrix, point behind the camera) |

See [Camera](../projection/camera.md) for the semantics of each operation.

---

## Notes

* **Float vectors only.** `Vec2`/`Vec3`/`Vec4` bind the `float` instantiations. Everything that takes or returns vectors in Lua uses them.
* **Unreal is double precision.** `unreal_engine::Camera` is built on `Vector3<double>` and a double-precision pitch type, which are not the types `Vec3` and the shared angle types bind. The Unreal sub-table is registered, but no Lua test covers it; check it against your build before relying on it.
* **Indices.** Matrix `at`/`set_at` are 0-based; every Lua table you pass or receive (`from_rows`, polyline points, collider vertices, `vertices()`, `as_table()` of a canvas box) is 1-based.
* **Errors.** `world_to_screen`, `screen_to_world` and `angle_between` return `nil, message` rather than raising. Programmer errors (out-of-range matrix index, bad row table, empty convex collider, `nil` HUD renderer) raise an error.
* **Lifetime.** The HUD `Renderer` is held as a shared pointer, so it can safely outlive the Lua local that created it. `EntityOverlay` stores the renderer and your callback table; keep the Lua state alive while overlays exist.
* **Tests.** The scripts under `tests/lua/*.lua` double as usage examples for every area above.

---

## See also

* [Camera](../projection/camera.md)
* [HUD overview](../hud/overview.md)
* [Vector3](../linear_algebra/vector3.md)
* [Color](../utility/color.md)
* [Pattern scan](../utility/pattern_scan.md)
