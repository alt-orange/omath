# `omath::unreal_engine` — formulas & matrix helpers

> Header: `omath/engines/unreal_engine/formulas.hpp`
> Namespace: `omath::unreal_engine`
> Purpose: compute direction vectors, rotation matrices, view matrices, and perspective projections for Unreal Engine

---

## Summary

This header provides **Unreal Engine**-specific math for:

* **Direction vectors** (`forward`, `right`, `up`) from `ViewAngles`
* **Rotation matrices** from Euler angles
* **View matrices** (camera transforms)
* **Perspective projection** matrices

All functions respect Unreal Engine's **Z-up, left-handed** coordinate system.

---

## API

```cpp
namespace omath::unreal_engine {

  // Compute forward direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<double> forward_vector(const ViewAngles& angles) noexcept;

  // Compute right direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<double> right_vector(const ViewAngles& angles) noexcept;

  // Compute up direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<double> up_vector(const ViewAngles& angles) noexcept;

  // Build 4x4 rotation matrix from angles (mat_rotation_zyx(yaw, -pitch, -roll))
  [[nodiscard]]
  constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

  // Decompose a transform matrix
  [[nodiscard]] constexpr Vector3<double> extract_origin(const Mat4X4& mat) noexcept;
  [[nodiscard]] constexpr Vector3<double> extract_scale(const Mat4X4& mat) noexcept;
  [[nodiscard]] constexpr ViewAngles      extract_rotation_angles(const Mat4X4& mat) noexcept;

  // Build view matrix (camera space transform)
  [[nodiscard]]
  constexpr Mat4X4 calc_view_matrix(const ViewAngles& angles,
                                    const Vector3<double>& cam_origin) noexcept;

  // Build perspective projection matrix
  [[nodiscard]]
  constexpr Mat4X4 calc_perspective_projection_matrix(
      double field_of_view, double aspect_ratio, double near_plane, double far_plane,
      NDCDepthRange ndc_depth_range = NDCDepthRange::NEGATIVE_ONE_TO_ONE) noexcept;

  // Unit conversions (FloatingType = any floating-point type)
  template<class FloatingType> constexpr FloatingType units_to_centimeters(const FloatingType& units);
  template<class FloatingType> constexpr FloatingType units_to_meters(const FloatingType& units);
  template<class FloatingType> constexpr FloatingType units_to_kilometers(const FloatingType& units);
  template<class FloatingType> constexpr FloatingType centimeters_to_units(const FloatingType& centimeters);
  template<class FloatingType> constexpr FloatingType meters_to_units(const FloatingType& meters);
  template<class FloatingType> constexpr FloatingType kilometers_to_units(const FloatingType& kilometers);

} // namespace omath::unreal_engine
```

---

## Direction vectors

Given camera angles (pitch/yaw/roll):

* `forward_vector(angles)` → unit vector pointing where the camera looks
* `right_vector(angles)` → unit vector pointing to the camera's right
* `up_vector(angles)` → unit vector pointing upward relative to the camera

These are used for movement, aim direction, and building coordinate frames.

---

## Rotation & view matrices

* `rotation_matrix(angles)` → 4×4 rotation matrix from Euler angles (`mat_rotation_zyx(yaw, -pitch, -roll)`)
* `calc_view_matrix(angles, origin)` → camera view matrix
* `extract_origin`, `extract_scale`, `extract_rotation_angles` → read the translation, scale and Euler angles back out of a transform matrix (the inverse of the above)

The view matrix transforms world coordinates into camera space (origin at camera, axes aligned with camera orientation).

---

## Perspective projection

```cpp
Mat4X4 proj = calc_perspective_projection_matrix(
  fov_degrees,    // horizontal field of view (e.g., 90)
  aspect_ratio,   // width / height (e.g., 16/9)
  near_plane,     // e.g., 10.0
  far_plane,      // e.g., 100000.0
  NDCDepthRange::ZERO_TO_ONE // optional, defaults to NEGATIVE_ONE_TO_ONE
);
```

All parameters are `double`. The matrix is a left-handed, row-major, **horizontal**-FOV perspective matrix (`mat_perspective_left_handed_horizontal_fov`); `ndc_depth_range` selects the depth range of the resulting clip space.

Produces a **perspective projection matrix** suitable for 3D rendering pipelines. Combined with the view matrix, this implements the standard camera transform chain.

---

## Usage example

```cpp
using namespace omath::unreal_engine;

// Camera setup
ViewAngles angles = {
  PitchAngle::from_degrees(-20.0),
  YawAngle::from_degrees(45.0),
  RollAngle::from_degrees(0.0)
};
Vector3<double> cam_pos{1000.0, 500.0, 200.0};

// Compute direction
auto forward = forward_vector(angles);
auto right   = right_vector(angles);
auto up      = up_vector(angles);

// Build matrices
auto view_mat = calc_view_matrix(angles, cam_pos);
auto proj_mat = calc_perspective_projection_matrix(90.0, 16.0/9.0, 10.0, 100000.0);

// Use view_mat and proj_mat for rendering...
```

---

## Unit conversions

One Unreal unit is one centimeter: `units_to_centimeters` / `centimeters_to_units` return the value unchanged, `units_to_meters` divides by 100 and `meters_to_units` multiplies by 100. `units_to_kilometers` / `kilometers_to_units` add a further factor of 1000. They are `constexpr` templates restricted to floating-point types.

---

## Conventions

* **Angles**: pitch (up/down), yaw (left/right), roll (tilt)
* **Pitch**: positive = looking up, negative = looking down
* **Yaw**: increases counter-clockwise from the +X axis
* **Coordinate system**: Z-up, X-forward, Y-right (left-handed)

---

## See also

* `omath/engines/unreal_engine/constants.hpp` — coordinate frame & angle types
* `omath/engines/unreal_engine/traits/camera_trait.hpp` — plug-in for generic `Camera`
* `omath/projection/camera.hpp` — generic camera wrapper using these formulas
