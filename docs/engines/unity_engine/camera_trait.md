# `omath::unity_engine::CameraTrait` — plug-in trait for `projection::Camera`

> Header: `omath/engines/unity_engine/traits/camera_trait.hpp` (header-only)
> Namespace: `omath::unity_engine`
> Purpose: provide Unity Engine-style **look-at**, **view**, and **projection** math to the generic `omath::projection::Camera` (satisfies `CameraEngineConcept`).

---

## Summary

`CameraTrait` exposes four `static constexpr` functions:

* `calc_look_at_angle(origin, look_at)` – computes Euler angles so the camera at `origin` looks at `look_at`. Implementation normalizes the direction, computes **pitch** as `-asin(dir.y)` and **yaw** as `atan2(dir.x, dir.z)`; **roll** is `0`. Pitch/yaw/roll are returned using the project's strong angle types (`PitchAngle`, `YawAngle`, `RollAngle`).
* `calc_view_matrix(angles, origin)` – delegates to Unity Engine formulas `unity_engine::calc_view_matrix`, producing a `Mat4X4` view matrix for the given angles and origin.
* `extract_rotation_angles(rotation)` – inverse of `rotation_matrix()` (delegates to `unity_engine::extract_rotation_angles`); lets `Camera::calc_view_angles_from_view_matrix()` recover all three angles.
* `calc_projection_matrix(fov, viewport, near, far, ndc_depth_range)` – builds a perspective projection by calling `calc_perspective_projection_matrix(fov_degrees, aspect, near, far, ndc_depth_range)`, where `aspect = viewport.aspect_ratio()`. Accepts `FieldOfView` (degrees).

The trait's types (`ViewAngles`, `Mat4X4`, angle aliases) and helpers live in the Unity Engine math headers included by the trait (`formulas.hpp`) and the shared projection header (`projection/camera.hpp`).

---

## API

```cpp
namespace omath::unity_engine {

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
};

} // namespace omath::unity_engine
```

Uses: `Vector3<float>`, `ViewAngles` (pitch/yaw/roll), `Mat4X4`, `projection::FieldOfView`, `projection::ViewPort`.

---

## Behavior & conventions

* **Angles from look-at** (Y-up coordinate system):

  ```
  dir = normalize(look_at - origin)
  pitch = -asin(dir.y)         // +Y is up, but negative pitch looks up
  yaw   = atan2(dir.x, dir.z)  // horizontal rotation
  roll  = 0
  ```

  Built with `PitchAngle::from_asin(...)`, `YawAngle::from_atan2(...)` and `RollAngle::from_radians(0)`.

* **View matrix**: built by the Unity Engine helper `unity_engine::calc_view_matrix(angles, origin)` to match the engine's handedness and axis conventions.

* **Projection**: uses `calc_perspective_projection_matrix(fov.as_degrees(), viewport.aspect_ratio(), near, far, ndc_depth_range)`. Pass your **vertical FOV** in degrees via `FieldOfView`; the helper builds a right-handed vertical-FOV perspective matrix (the view matrix negates the forward axis to match). Unlike the Source/IW traits there is no `calc_field_of_view`.

---

## Using with `projection::Camera`

Create a camera whose math is driven by this trait:

```cpp
// omath/engines/unity_engine/camera.hpp already provides this alias:
//   using Camera = projection::Camera<Mat4X4, ViewAngles, CameraTrait,
//                                     NDCDepthRange::ZERO_TO_ONE, {.inverted_forward = true}>;
using UEcam = omath::unity_engine::Camera;

omath::projection::ViewPort vp{1920.f, 1080.f};
auto fov = omath::projection::FieldOfView::from_degrees(60.f);

UEcam cam(
  /*position*/   {10.f, 5.f, -10.f},
  /*angles*/     omath::unity_engine::CameraTrait::calc_look_at_angle({10,5,-10},{0,5,0}),
  /*viewport*/   vp,
  /*fov*/        fov,
  /*near*/       0.3f,
  /*far*/        1000.f
);
```

This satisfies `CameraEngineConcept` expected by `projection::Camera` (look-at, view, projection) as declared in the trait header.

---

## Notes & tips

* The `PitchAngle` (clamped to ±90°), `YawAngle` and `RollAngle` aliases come from [constants](constants.md); `calc_look_at_angle` builds them from `asin`/`atan2`/radians.
* `aspect_ratio()` is taken directly from `ViewPort` (`width / height`), so keep both positive and non-zero.
* `near` must be > 0 and `< far` for a valid projection matrix (enforced by your math helpers).
* Unity Engine uses **Y-up**: pitch angles control vertical look; **negative pitch = up**, positive = down.

---

## See also

* [Unity Engine Formulas](formulas.md) - view/projection builders used above.
* [Unity Engine Constants](constants.md) - coordinate frame and angle types.
* Generic camera wrapper `omath::projection::Camera` and its `CameraEngineConcept` (this trait is designed to plug straight into it).
