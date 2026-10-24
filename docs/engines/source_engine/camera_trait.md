# `omath::source_engine::CameraTrait` — plug-in trait for `projection::Camera`

> Header: `omath/engines/source_engine/traits/camera_trait.hpp` (header-only)
> Namespace: `omath::source_engine`
> Purpose: provide Source Engine-style **look-at**, **view**, and **projection** math to the generic `omath::projection::Camera` (satisfies `CameraEngineConcept`).

---

## Summary

`CameraTrait` exposes five `static constexpr` functions:

* `calc_look_at_angle(origin, look_at)` – computes Euler angles so the camera at `origin` looks at `look_at`. Implementation normalizes the direction, computes **pitch** as `-asin(dir.z)` and **yaw** as `atan2(dir.y, dir.x)`; **roll** is `0`. Pitch/yaw/roll are returned using the project's strong angle types (`PitchAngle`, `YawAngle`, `RollAngle`).
* `calc_view_matrix(angles, origin)` – delegates to Source Engine formulas `source_engine::calc_view_matrix`, producing a `Mat4X4` view matrix for the given angles and origin.
* `extract_rotation_angles(rotation)` – inverse of `rotation_matrix()` (delegates to `source_engine::extract_rotation_angles`); lets `Camera::calc_view_angles_from_view_matrix()` recover all three angles.
* `calc_projection_matrix(fov, viewport, near, far, ndc_depth_range)` – builds a perspective projection by calling `calc_perspective_projection_matrix(fov_degrees, aspect, near, far, ndc_depth_range)`, where `aspect = viewport.aspect_ratio()`. Accepts `FieldOfView` (degrees).
* `calc_field_of_view(projection_matrix)` – recovers the `FieldOfView` from a projection matrix: the vertical FOV is read from `m[1,1]` and widened back to the horizontal FOV at the 4:3 reference aspect ratio.

The trait's types (`ViewAngles`, `Mat4X4`, angle aliases) and helpers live in the Source Engine math headers included by the trait (`formulas.hpp`) and the shared projection header (`projection/camera.hpp`).

---

## API

```cpp
namespace omath::source_engine {

class CameraTrait final {
public:
  // Compute Euler angles (pitch/yaw/roll) to look from cam_origin to look_at.
  constexpr static ViewAngles
  calc_look_at_angle(const Vector3<float>& cam_origin,
                     const Vector3<float>& look_at) noexcept;

  // Build view matrix for given angles and origin.
  constexpr static Mat4X4
  calc_view_matrix(const ViewAngles& angles,
                   const Vector3<float>& cam_origin) noexcept;

  // Inverse of rotation_matrix(): recover pitch/yaw/roll.
  constexpr static ViewAngles
  extract_rotation_angles(const Mat4X4& rotation) noexcept;

  // Build perspective projection from FOV (deg), viewport, near/far.
  constexpr static Mat4X4
  calc_projection_matrix(const projection::FieldOfView& fov,
                         const projection::ViewPort& view_port,
                         float near_plane, float far_plane,
                         NDCDepthRange ndc_depth_range) noexcept;

  // Recover the (horizontal, 4:3 reference) FOV from a projection matrix.
  constexpr static projection::FieldOfView
  calc_field_of_view(const Mat4X4& projection_matrix) noexcept;
};

} // namespace omath::source_engine
```

Uses: `Vector3<float>`, `ViewAngles` (pitch/yaw/roll), `Mat4X4`, `projection::FieldOfView`, `projection::ViewPort`.

---

## Behavior & conventions

* **Angles from look-at** (Z-up coordinate system):

  ```
  dir = normalize(look_at - origin)
  pitch = -asin(dir.z)         // +Z is up, but negative pitch looks up
  yaw   = atan2(dir.y, dir.x)  // horizontal rotation
  roll  = 0
  ```

  Built with `PitchAngle::from_asin(...)`, `YawAngle::from_atan2(...)` and `RollAngle::from_radians(0)`.

* **View matrix**: built by the Source Engine helper `source_engine::calc_view_matrix(angles, origin)` to match the engine's handedness and axis conventions.

* **Projection**: uses `calc_perspective_projection_matrix(fov.as_degrees(), viewport.aspect_ratio(), near, far, ndc_depth_range)`. Pass the engine's **horizontal FOV at a 4:3 reference aspect ratio** (`k_fov_reference_aspect_ratio`) in degrees via `FieldOfView`; the helper converts it to a vertical FOV and builds a left-handed perspective matrix. `calc_field_of_view` is the inverse of this.

---

## Using with `projection::Camera`

Create a camera whose math is driven by this trait:

```cpp
// omath/engines/source_engine/camera.hpp already provides this alias:
//   using Camera = projection::Camera<Mat4X4, ViewAngles, CameraTrait, NDCDepthRange::ZERO_TO_ONE>;
using SEcam = omath::source_engine::Camera;

omath::projection::ViewPort vp{1920.f, 1080.f};
auto fov = omath::projection::FieldOfView::from_degrees(90.f);

SEcam cam(
  /*position*/   {100.f, 50.f, 80.f},
  /*angles*/     omath::source_engine::CameraTrait::calc_look_at_angle({100,50,80},{0,0,80}),
  /*viewport*/   vp,
  /*fov*/        fov,
  /*near*/       0.1f,
  /*far*/        1000.f
);
```

This satisfies `CameraEngineConcept` expected by `projection::Camera` (look-at, view, projection) as declared in the trait header.

---

## Notes & tips

* The `PitchAngle` (clamped to ±89°), `YawAngle` and `RollAngle` aliases come from [constants](constants.md); `calc_look_at_angle` builds them from `asin`/`atan2`/radians.
* `aspect_ratio()` is taken directly from `ViewPort` (`width / height`), so keep both positive and non-zero.
* `near` must be > 0 and `< far` for a valid projection matrix (enforced by your math helpers).
* Source Engine uses **Z-up**: pitch angles control vertical look; **negative pitch = up**, positive = down.

---

## See also

* [Source Engine Formulas](formulas.md) - View/projection matrix builders
* [Source Engine Constants](constants.md) - Engine-specific constants
* [Source Engine Pred Engine Trait](pred_engine_trait.md) - Projectile prediction for Source Engine
* [Generic Camera Documentation](../../projection/camera.md) - Camera base class
* [Getting Started Guide](../../getting_started.md) - Quick start with OMath
* [Tutorials - World-to-Screen](../../tutorials.md#tutorial-2-world-to-screen-projection) - Projection tutorial
