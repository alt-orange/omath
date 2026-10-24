# `omath::opengl_engine` — formulas & matrix helpers

> Header: `omath/engines/opengl_engine/formulas.hpp`
> Namespace: `omath::opengl_engine`
> Purpose: compute direction vectors, rotation matrices, view matrices, and perspective projections for OpenGL

---

## Summary

This header provides **OpenGL**-specific math for:

* **Direction vectors** (`forward`, `right`, `up`) from `ViewAngles`
* **Rotation matrices** from Euler angles
* **View matrices** (camera transforms)
* **Perspective projection** matrices

All functions respect OpenGL's **Y-up, right-handed** coordinate system with **forward = -Z**.

---

## API

```cpp
namespace omath::opengl_engine {

  // Compute forward direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<float> forward_vector(const ViewAngles& angles) noexcept;

  // Compute right direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<float> right_vector(const ViewAngles& angles) noexcept;

  // Compute up direction from pitch/yaw/roll
  [[nodiscard]]
  constexpr Vector3<float> up_vector(const ViewAngles& angles) noexcept;

  // Build 4x4 rotation matrix from angles (mat_rotation_yxz: yaw * pitch * roll, column-major)
  [[nodiscard]]
  constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

  // Decompose a transform matrix
  [[nodiscard]] constexpr Vector3<float> extract_origin(const Mat4X4& mat) noexcept;
  [[nodiscard]] constexpr Vector3<float> extract_scale(const Mat4X4& mat) noexcept;
  [[nodiscard]] constexpr ViewAngles     extract_rotation_angles(const Mat4X4& mat) noexcept;

  // Build view matrix (camera space transform, mat_look_at_right_handed)
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

} // namespace omath::opengl_engine
```

---

## Direction vectors

Given camera angles (pitch/yaw/roll):

* `forward_vector(angles)` → unit vector pointing where the camera looks (typically `-Z` direction)
* `right_vector(angles)` → unit vector pointing to the camera's right (`+X` direction)
* `up_vector(angles)` → unit vector pointing upward relative to the camera (`+Y` direction)

These are used for movement, aim direction, and building coordinate frames.

---

## Rotation & view matrices

* `rotation_matrix(angles)` → 4×4 rotation matrix from Euler angles (`mat_rotation_yxz(yaw, pitch, roll)`, column-major; roll turns around the camera's own forward axis)
* `calc_view_matrix(angles, origin)` → camera view matrix (column-major, built with `mat_look_at_right_handed` from the rotated forward and up)
* `extract_origin`, `extract_scale`, `extract_rotation_angles` → read the translation, scale and Euler angles back out of a transform matrix (the inverse of the above)

The view matrix transforms world coordinates into camera space (origin at camera, axes aligned with camera orientation).

**Note**: Matrices are **column-major** to match OpenGL/GLSL conventions. No transpose needed when uploading to shaders.

---

## Perspective projection

```cpp
Mat4X4 proj = calc_perspective_projection_matrix(
  fov_degrees,    // vertical field of view (e.g., 45)
  aspect_ratio,   // width / height (e.g., 16/9)
  near_plane,     // e.g., 0.1
  far_plane,      // e.g., 100.0
  NDCDepthRange::NEGATIVE_ONE_TO_ONE // optional, this is the default
);
```

The matrix is a right-handed, column-major, vertical-FOV perspective matrix (`mat_perspective_right_handed_vertical_fov`); `ndc_depth_range` selects the depth range of the resulting clip space.

Produces a **perspective projection matrix** suitable for OpenGL rendering. Combined with the view matrix, this implements the standard camera transform chain.

---

## Usage example

```cpp
using namespace omath::opengl_engine;

// Camera setup
ViewAngles angles = {
  PitchAngle::from_degrees(-20.0f),
  YawAngle::from_degrees(135.0f),
  RollAngle::from_degrees(0.0f)
};
Vector3<float> cam_pos{5.0f, 3.0f, 5.0f};

// Compute direction
auto forward = forward_vector(angles);
auto right   = right_vector(angles);
auto up      = up_vector(angles);

// Build matrices (column-major for OpenGL)
auto view_mat = calc_view_matrix(angles, cam_pos);
auto proj_mat = calc_perspective_projection_matrix(45.0f, 16.0f/9.0f, 0.1f, 100.0f);

// Upload to OpenGL shaders (no transpose needed)
glUniformMatrix4fv(view_loc, 1, GL_FALSE, view_mat.raw_array().data());
glUniformMatrix4fv(proj_loc, 1, GL_FALSE, proj_mat.raw_array().data());
```

---

## Unit conversions

One OpenGL unit is one meter: `units_to_meters` / `meters_to_units` return the value unchanged, `units_to_centimeters` multiplies by 100 and `centimeters_to_units` divides by 100. `units_to_kilometers` / `kilometers_to_units` scale by 1000. They are `constexpr` templates restricted to floating-point types.

---

## Conventions

* **Angles**: pitch (up/down), yaw (left/right), roll (tilt)
* **Pitch**: positive = looking up, negative = looking down
* **Yaw**: increases counter-clockwise from the -Z axis
* **Coordinate system**: Y-up, -Z-forward, X-right (right-handed)
* **Matrix storage**: column-major (matches OpenGL/GLSL)

---

## See also

* `omath/engines/opengl_engine/constants.hpp` — coordinate frame & angle types
* `omath/engines/opengl_engine/traits/camera_trait.hpp` — plug-in for generic `Camera`
* `omath/projection/camera.hpp` — generic camera wrapper using these formulas
