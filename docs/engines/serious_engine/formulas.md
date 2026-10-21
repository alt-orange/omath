# `omath::serious_engine` — formulas & matrix helpers

> Header: `omath/engines/serious_engine/formulas.hpp`
> Namespace: `omath::serious_engine`
> Purpose: direction vectors, rotation/view/projection matrices and unit conversions for Serious Engine

---

## API

```cpp
namespace omath::serious_engine {

  Vector3<float> forward_vector(const ViewAngles& angles) noexcept;
  Vector3<float> right_vector(const ViewAngles& angles) noexcept;
  Vector3<float> up_vector(const ViewAngles& angles) noexcept;

  Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

  Vector3<float> extract_origin(const Mat4X4& mat) noexcept;
  Vector3<float> extract_scale(const Mat4X4& mat) noexcept;
  ViewAngles     extract_rotation_angles(const Mat4X4& mat) noexcept;

  Mat4X4 calc_view_matrix(const ViewAngles& angles, const Vector3<float>& cam_origin) noexcept;

  Mat4X4 calc_perspective_projection_matrix(float field_of_view, // horizontal, degrees
                                            float aspect_ratio, float near, float far,
                                            NDCDepthRange ndc_depth_range = NDCDepthRange::ZERO_TO_ONE) noexcept;

  // 1 unit = 1 meter
  template<class T> T units_to_centimeters(const T& units);   // * 100
  template<class T> T units_to_meters(const T& units);        // identity
  template<class T> T units_to_kilometers(const T& units);    // / 1000
  template<class T> T centimeters_to_units(const T& cm);
  template<class T> T meters_to_units(const T& m);
  template<class T> T kilometers_to_units(const T& km);

} // namespace omath::serious_engine
```

---

## Rotation matrix

`rotation_matrix` is `Ry(heading) · Rx(pitch) · Rz(banking)`, entry for entry the same as `MakeRotationMatrix()` in the
engine's `Sources/Engine/Math/Geometry.cpp`. Banking turns around the camera's own forward axis.

`extract_rotation_angles` inverts it. At pitch ±90° heading and banking turn around the same axis, so banking is set to
0 and the whole rotation goes into heading; the resulting orientation is the same.

---

## View matrix

`calc_view_matrix` returns rows `right | tx`, `up | ty`, `back | tz` (back = -forward) — the same layout as the
engine's own 3×4 view matrix, so a matrix read from the game can be compared directly.

---

## Projection

The engine's FOV is **horizontal** (`ppr_FOVWidth` in `Sources/Engine/Math/Projection_Perspective.cpp`):

* X scale = `1 / tan(fov / 2)`
* Y scale = X scale × aspect ratio

So changing the window's aspect ratio keeps the horizontal view and changes the vertical one. The default depth range is
`ZERO_TO_ONE` (Serious Sam HD renders through Direct3D 9).

---

## Usage example

```cpp
using namespace omath::serious_engine;

const ViewAngles angles{PitchAngle::from_degrees(-14.6f), YawAngle::from_degrees(144.6f),
                        RollAngle::from_degrees(0.f)};

const auto forward = forward_vector(angles);
const auto view = calc_view_matrix(angles, {-17.2f, -22.0f, -520.0f});
const auto proj = calc_perspective_projection_matrix(90.f, 1280.f / 1024.f, 0.2f, 10000.f);
```

---

## See also

* [Constants](constants.md) — coordinate frame & angle types
* [Camera Trait](camera_trait.md) — plug-in for `projection::Camera`
