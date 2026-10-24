# `omath::iw_engine` — formulas & matrix helpers

> Header: `omath/engines/iw_engine/formulas.hpp`
> Namespace: `omath::iw_engine`
> Purpose: compute direction vectors, rotation matrices, view matrices, and perspective projections for IW Engine (Call of Duty)

---

## Summary

This header provides **IW Engine**-specific math for:

* **Direction vectors** (`forward`, `right`, `up`) from `ViewAngles`
* **Rotation matrices** from Euler angles
* **View matrices** (camera transforms)
* **Perspective projection** matrices

All functions respect IW Engine's **Z-up, right-handed** coordinate system.

---

## API

```cpp
namespace omath::iw_engine {

  // Compute forward direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<float> forward_vector(const ViewAngles& angles) noexcept;

  // Compute right direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<float> right_vector(const ViewAngles& angles) noexcept;

  // Compute up direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<float> up_vector(const ViewAngles& angles) noexcept;

  // Build 4x4 rotation matrix from angles (mat_rotation_zyx: yaw, pitch, roll)
  [[nodiscard]]
  constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

  // Decompose a transform matrix
  [[nodiscard]] constexpr Vector3<float> extract_origin(const Mat4X4& mat) noexcept;
  [[nodiscard]] constexpr Vector3<float> extract_scale(const Mat4X4& mat) noexcept;
  [[nodiscard]] constexpr ViewAngles     extract_rotation_angles(const Mat4X4& mat) noexcept;

  // Build view matrix (camera space transform)
  [[nodiscard]]
  constexpr Mat4X4 calc_view_matrix(const ViewAngles& angles,
                                    const Vector3<float>& cam_origin) noexcept;

  // Build perspective projection matrix
  [[nodiscard]]
  constexpr Mat4X4 calc_perspective_projection_matrix(
      float field_of_view, float aspect_ratio, float near_plane, float far_plane,
      NDCDepthRange ndc_depth_range = NDCDepthRange::NEGATIVE_ONE_TO_ONE) noexcept;

  // Unit conversions (FloatingType = any floating-point type)
  template<class FloatingType> constexpr FloatingType units_to_centimeters(const FloatingType& units);
  template<class FloatingType> constexpr FloatingType units_to_meters(const FloatingType& units);
  template<class FloatingType> constexpr FloatingType units_to_kilometers(const FloatingType& units);
  template<class FloatingType> constexpr FloatingType centimeters_to_units(const FloatingType& centimeters);
  template<class FloatingType> constexpr FloatingType meters_to_units(const FloatingType& meters);
  template<class FloatingType> constexpr FloatingType kilometers_to_units(const FloatingType& kilometers);

} // namespace omath::iw_engine
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

* `rotation_matrix(angles)` → 4×4 rotation matrix from Euler angles (`mat_rotation_zyx(yaw, pitch, roll)`)
* `calc_view_matrix(angles, origin)` → camera view matrix (the rotation is built once and its forward/right/up columns are reused)
* `extract_origin`, `extract_scale`, `extract_rotation_angles` → read the translation, scale and Euler angles back out of a transform matrix (the inverse of the above)

The view matrix transforms world coordinates into camera space (origin at camera, axes aligned with camera orientation).

---

## Perspective projection

```cpp
Mat4X4 proj = calc_perspective_projection_matrix(
  fov_degrees,    // horizontal FOV at a 4:3 aspect ratio (e.g., 65)
  aspect_ratio,   // width / height (e.g., 16/9)
  near_plane,     // e.g., 0.1
  far_plane,      // e.g., 5000.0
  NDCDepthRange::ZERO_TO_ONE // optional, defaults to NEGATIVE_ONE_TO_ONE
);
```

The FOV is interpreted as **horizontal at `k_fov_reference_aspect_ratio` (4:3)**, whatever `aspect_ratio` is; it is converted to a vertical FOV internally. The matrix is left-handed and row-major, and `ndc_depth_range` selects the depth range of the resulting clip space. Produces a **perspective projection matrix** suitable for 3D rendering pipelines. Combined with the view matrix, this implements the standard camera transform chain.

---

## Usage example

```cpp
using namespace omath::iw_engine;

// Camera setup
ViewAngles angles = {
  PitchAngle::from_degrees(-10.0f),
  YawAngle::from_degrees(90.0f),
  RollAngle::from_degrees(0.0f)
};
Vector3<float> cam_pos{500.0f, 200.0f, 100.0f};

// Compute direction
auto forward = forward_vector(angles);
auto right   = right_vector(angles);
auto up      = up_vector(angles);

// Build matrices
auto view_mat = calc_view_matrix(angles, cam_pos);
auto proj_mat = calc_perspective_projection_matrix(65.0f, 16.0f/9.0f, 0.1f, 5000.0f);

// Use view_mat and proj_mat for rendering...
```

---

## Unit conversions

One IW unit is 2.54 cm (`units_to_centimeters(u) = u * 2.54`). `units_to_meters`/`units_to_kilometers` and the inverse `*_to_units` functions chain from that factor. They are `constexpr` templates restricted to floating-point types.

---

## Conventions

* **Angles**: pitch (up/down), yaw (left/right), roll (tilt)
* **Pitch**: negative = looking up, positive = looking down (clamped to ±89°)
* **Yaw**: increases counter-clockwise from the +X axis
* **Coordinate system**: Z-up, X-forward, Y-right (negative in code convention)

---

## See also

* `omath/engines/iw_engine/constants.hpp` — coordinate frame & angle types
* `omath/engines/iw_engine/traits/camera_trait.hpp` — plug-in for generic `Camera`
* `omath/projection/camera.hpp` — generic camera wrapper using these formulas
