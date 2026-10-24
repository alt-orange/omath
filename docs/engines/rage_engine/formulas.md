# `omath::rage_engine` — formulas & matrix helpers

> Header: `omath/engines/rage_engine/formulas.hpp`
> Namespace: `omath::rage_engine`
> Purpose: direction vectors, rotation/view/projection matrices and unit conversions for RAGE

---

## API

```cpp
namespace omath::rage_engine {

  constexpr Vector3<float> forward_vector(const ViewAngles& angles) noexcept;
  constexpr Vector3<float> right_vector(const ViewAngles& angles) noexcept;
  constexpr Vector3<float> up_vector(const ViewAngles& angles) noexcept;

  constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

  constexpr Vector3<float> extract_origin(const Mat4X4& mat) noexcept;
  constexpr Vector3<float> extract_scale(const Mat4X4& mat) noexcept;
  constexpr ViewAngles     extract_rotation_angles(const Mat4X4& mat) noexcept;

  constexpr Mat4X4 calc_view_matrix(const ViewAngles& angles, const Vector3<float>& cam_origin) noexcept;

  constexpr Mat4X4 calc_perspective_projection_matrix(float field_of_view, // vertical, degrees
                                                      float aspect_ratio, float near, float far,
                                                      NDCDepthRange ndc_depth_range = NDCDepthRange::ZERO_TO_ONE) noexcept;

  template<class T> T units_to_centimeters(const T& units);   // / 100
  template<class T> T units_to_meters(const T& units);        // identity
  template<class T> T units_to_kilometers(const T& units);    // / 1000
  template<class T> T centimeters_to_units(const T& cm);      // * 100
  template<class T> T meters_to_units(const T& m);            // identity
  template<class T> T kilometers_to_units(const T& km);       // * 1000

} // namespace omath::rage_engine
```

All functions are `[[nodiscard]]`. Everything except the unit conversions is `noexcept`.

---

## Rotation matrix

`rotation_matrix` is `mat_rotation_zxy(yaw, pitch, roll)`, i.e. `Rz(yaw) · Rx(pitch) · Ry(roll)`: roll is applied first,
around the camera's own forward (`+Y`) axis, then pitch, then yaw around the world `Z` axis.

`forward_vector`, `right_vector` and `up_vector` rotate `k_abs_forward`, `k_abs_right` and `k_abs_up` (see
[Constants](constants.md)) by that matrix. With all angles at 0 they return exactly the `k_abs_*` axes. For a level
camera, positive yaw turns towards `-X`: yaw 90 degrees gives a forward vector of `{-1, 0, 0}`.

`extract_rotation_angles` inverts it (via `mat_extract_rotation_zxy`, ignoring scale): pitch comes from the `x`
component, yaw from `z` and roll from `y`. At pitch +/-90 degrees yaw and roll turn around the same axis, so roll is set
to 0 and the whole rotation goes into yaw; the resulting orientation is the same.

`extract_origin` returns the translation column (`{m[0,3], m[1,3], m[2,3]}`) and `extract_scale` the lengths of the
three basis columns (a zero-length column is reported as scale 1).

---

## View matrix

`calc_view_matrix` builds the rotation once, takes the rotated forward, right and up axes, and returns (row-major):

```
| right.x    right.y    right.z    -dot(right,   origin) |
| up.x       up.y       up.z       -dot(up,      origin) |
| forward.x  forward.y  forward.z  -dot(forward, origin) |
| 0          0          0           1                    |
```

Row 2 is **forward** (not its negation), so view space is `x` right, `y` up, `z` forward. For all angles at 0 the
rotation block therefore has ones at `(0,0)`, `(1,2)` and `(2,1)`, which maps world Z-up to view Y-up.

---

## Projection

`calc_perspective_projection_matrix` wraps `mat_perspective_left_handed_vertical_fov` (row-major, `w = z`):

* The FOV is **vertical**, in degrees. Y scale = `1 / tan(fov / 2)`, X scale = Y scale / aspect ratio.
* `ZERO_TO_ONE` (the default): `m[2,2] = far / (far - near)`, `m[2,3] = -near * far / (far - near)`.
* `NEGATIVE_ONE_TO_ONE`: `m[2,2] = (far + near) / (far - near)`, `m[2,3] = -2 * near * far / (far - near)`.
* `m[3,2] = 1`.

For `fov = 90`, `aspect = 1`, `near = 1`, `far = 11` and `ZERO_TO_ONE`, the unit tests expect `m[0,0] = m[1,1] = 1`,
`m[2,2] = 1.1`, `m[2,3] = -1.1` and `m[3,2] = 1`.

The camera alias `rage_engine::Camera` uses `NDCDepthRange::ZERO_TO_ONE`.

---

## Unit conversions

All six are `constexpr` and constrained to floating-point types.

!!! warning "Centimeter conversions are inverted relative to the meter ones"
    `units_to_meters` is the identity (a unit is a meter), yet `units_to_centimeters` **divides** by 100 and
    `centimeters_to_units` **multiplies** by 100. For a meter-based unit the centimeter value should be `units * 100`
    (as `cry_engine::units_to_centimeters` does). The two families only agree if a unit is a centimeter, which
    `units_to_meters` contradicts. The page documents the code as written; check the units of your data before relying
    on the centimeter helpers.

---

## Usage example

```cpp
using namespace omath::rage_engine;

const ViewAngles angles{PitchAngle::from_degrees(-10.f), YawAngle::from_degrees(45.f), RollAngle::from_degrees(0.f)};

const auto forward = forward_vector(angles);
const auto view = calc_view_matrix(angles, {100.f, 200.f, 1.7f});
const auto proj = calc_perspective_projection_matrix(60.f, 1920.f / 1080.f, 0.1f, 5000.f);
```

---

## See also

* [Constants](constants.md) — coordinate frame & angle types
* [Camera Trait](camera_trait.md) — plug-in for `projection::Camera`
* [Mesh Trait](mesh_trait.md) — uses `rotation_matrix`
