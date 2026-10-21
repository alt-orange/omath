# `omath::frostbite_engine` — formulas & matrix helpers

> Header: `omath/engines/frostbite_engine/formulas.hpp`
> Namespace: `omath::frostbite_engine`
> Purpose: direction vectors, rotation/view/projection matrices and unit conversions for Frostbite

---

## API

```cpp
namespace omath::frostbite_engine {

  Vector3<float> forward_vector(const ViewAngles& angles) noexcept;
  Vector3<float> right_vector(const ViewAngles& angles) noexcept;
  Vector3<float> up_vector(const ViewAngles& angles) noexcept;

  Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

  Vector3<float> extract_origin(const Mat4X4& mat) noexcept;
  Vector3<float> extract_scale(const Mat4X4& mat) noexcept;
  ViewAngles     extract_rotation_angles(const Mat4X4& mat) noexcept;

  Mat4X4 calc_view_matrix(const ViewAngles& angles, const Vector3<float>& cam_origin) noexcept;

  Mat4X4 calc_perspective_projection_matrix(float field_of_view, // vertical, degrees
                                            float aspect_ratio, float near, float far,
                                            NDCDepthRange ndc_depth_range = NDCDepthRange::NEGATIVE_ONE_TO_ONE) noexcept;

  // 1 unit = 1 meter
  template<class T> T units_to_centimeters(const T& units);   // * 100
  template<class T> T units_to_meters(const T& units);        // identity
  template<class T> T units_to_kilometers(const T& units);    // / 1000
  template<class T> T centimeters_to_units(const T& cm);
  template<class T> T meters_to_units(const T& m);
  template<class T> T kilometers_to_units(const T& km);

} // namespace omath::frostbite_engine
```

---

## Rotation matrix

`rotation_matrix` is `Ry(yaw) · Rx(pitch) · Rz(roll)` (`mat_rotation_yxz`): roll is applied first, around the camera's
own forward axis, then pitch, then yaw. Changing roll never changes where the camera looks, and the angle singularity
sits at pitch ±90° (looking straight up or down).

`extract_rotation_angles` (`mat_extract_rotation_yxz`) inverts it for any pitch in [-90°, 90°] and any yaw/roll.

---

## View & projection

* `calc_view_matrix(angles, origin)` — rows are right, up and forward (not negated), plus the translation.
* `calc_perspective_projection_matrix` — **left-handed**, **vertical** FOV in degrees.

---

## Usage example

```cpp
using namespace omath::frostbite_engine;

const ViewAngles angles{PitchAngle::from_degrees(-10.f), YawAngle::from_degrees(45.f), RollAngle::from_degrees(0.f)};

const auto forward = forward_vector(angles);
const auto view = calc_view_matrix(angles, {10.f, 2.f, -5.f});
const auto proj = calc_perspective_projection_matrix(60.f, 16.f / 9.f, 0.1f, 1000.f);
```

---

## See also

* [Constants](constants.md) — coordinate frame & angle types
* [Camera Trait](camera_trait.md) — plug-in for `projection::Camera`
