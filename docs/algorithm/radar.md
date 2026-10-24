# `omath::algorithm::world_to_radar` — project a world position onto a 2D radar

> Header: `omath/algorithm/radar.hpp`
> Namespace: `omath::algorithm`
> Depends on: [`Vector3`](../linear_algebra/vector3.md), [`Vector2`](../linear_algebra/vector2.md), a [`Camera`](../projection/camera.md) from any engine
> Purpose: turn a world-space point into a top-down offset around the camera, ready to draw on a minimap

---

## API

```cpp
namespace omath::algorithm {

template<class Camera, class FloatingType>
requires std::is_floating_point_v<FloatingType>
[[nodiscard]]
constexpr Vector2<float> world_to_radar(
    const Camera& camera,
    const Vector3<FloatingType>& position,
    const FloatingType scale,
    const std::optional<std::function<FloatingType(const Vector3<FloatingType>&,
                                                   const Vector3<FloatingType>&)>> calc_distance = std::nullopt);

} // namespace omath::algorithm
```

`Camera` is any type that provides `calc_look_at_angles(position)`, `get_view_angles()`, `get_origin()` and `get_abs_right()`; every `projection::Camera` instantiation does. `FloatingType` is deduced from `position` and `scale`, which must have the same type.

---

## Behavior

The result is an offset from the radar centre, in radar units, with the camera drawn at `(0, 0)`:

* **Up on the radar is "in front of the camera"**: a target straight ahead gives `y = -distance * scale` (negative `y`).
* **Right on the radar is "to the camera's right"**: a target on the right gives `x = +distance * scale`.

This is the screen convention of ImGui and most 2D canvases (`+x` right, `+y` down), so the offset can be added to the radar centre directly.

How it is computed:

1. The yaw between the camera's view direction and the direction to `position` is taken, offset by 90 degrees, and turned into a unit direction `(cos, sin)`.
2. A sign is derived from the camera's own right vector so that the result is mirrored correctly for engines with a different handedness. The unit tests check this against the Source, IW, Frostbite, OpenGL, Unity, Cry, Rage and Unreal cameras.
3. The unit direction is multiplied by `distance * scale`.

Properties that follow from the code:

* **Pitch and roll are ignored.** Looking up or down does not move blips; only yaw matters.
* **Default distance is the full 3D distance** `camera.get_origin().distance_to(position)`, so height difference makes a blip land further out. Pass `calc_distance` to change that.
* **`calc_distance(camera_origin, position)`** replaces the distance computation. For example, return the 2D distance to ignore height. Its return value is still multiplied by `scale`.
* **`scale`** converts world units to radar units. For a radar of pixel radius `R` that should cover `range` world units, use `scale = R / range`.
* The function does **not** clamp to the radar circle. Clamp or pin blips yourself.
* The return type is always `Vector2<float>`, whatever `FloatingType` is.

!!! note "Handedness sign"
    The handedness sign is computed once per template instantiation and cached in a function-local `static` on the first run-time call. In a `consteval`/constant-evaluated call it is computed every time. It is meant to encode the engine's handedness, so it does not depend on where the camera looks.

---

## Usage

Minimal call, from the `unit_test_radar` tests:

```cpp
#include <omath/algorithm/radar.hpp>
#include <omath/engines/source_engine/camera.hpp>

omath::source_engine::Camera camera{
    {10.f, -20.f, 5.f}, {}, {1280.f, 720.f},
    omath::projection::FieldOfView::from_degrees(90.f), 0.01f, 1000.f};

const omath::Vector3<float> enemy{40.f, -20.f, 5.f};
const auto blip = omath::algorithm::world_to_radar(camera, enemy, 0.5f);
```

Draw a blip on a radar and pin far targets to the rim (adapted from `examples/example_kf1_overlay`):

```cpp
const float radar_radius = 80.f;   // pixels
const float range = 4000.f;        // world units covered by the radar

auto offset = omath::algorithm::world_to_radar(camera, position, static_cast<double>(radar_radius / range));

const float length = offset.length();
if (length > radar_radius)
    offset *= radar_radius / length;          // keep the direction, put it on the rim

draw_dot({radar_center.x + offset.x, radar_center.y + offset.y});
```

Ignore height when measuring distance:

```cpp
using Vec = omath::Vector3<float>;

const std::optional<std::function<float(const Vec&, const Vec&)>> flat_distance =
    [](const Vec& from, const Vec& to) { return (to - from).length_2d(); };

const auto blip = omath::algorithm::world_to_radar(camera, enemy, 0.5f, flat_distance);
```

---

## Notes

* `scale` and `position` must share one floating-point type; mixing `float` and `double` fails deduction. Cast explicitly, as the example does with `static_cast<double>`.
* `length_2d()` in the last example measures in the `x`/`y` plane, which is the ground plane only for engines with a Z-up coordinate system; use the matching horizontal axes for your engine.

---

## See also

* [Camera](../projection/camera.md)
* [Targeting helpers](targeting.md)
* [HUD overview](../hud/overview.md)
