# `omath::rage_engine::CameraTrait` — plug-in trait for `projection::Camera`

> Header: `omath/engines/rage_engine/traits/camera_trait.hpp` (ready-made camera: `omath/engines/rage_engine/camera.hpp`)
> Namespace: `omath::rage_engine`
> Purpose: RAGE **look-at**, **view**, and **projection** math for the generic `omath::projection::Camera`

---

## API

```cpp
namespace omath::rage_engine {

class CameraTrait final {
public:
  static constexpr ViewAngles calc_look_at_angle(const Vector3<float>& cam_origin,
                                                 const Vector3<float>& look_at) noexcept;

  static constexpr Mat4X4 calc_view_matrix(const ViewAngles& angles,
                                           const Vector3<float>& cam_origin) noexcept;

  static constexpr Mat4X4 calc_projection_matrix(const projection::FieldOfView& fov, // vertical
                                                 const projection::ViewPort& view_port,
                                                 float near, float far,
                                                 NDCDepthRange ndc_depth_range) noexcept;

  // Used by Camera::calc_view_angles_from_view_matrix(): inverse of rotation_matrix()
  static constexpr ViewAngles extract_rotation_angles(const Mat4X4& rotation) noexcept;
};

} // namespace omath::rage_engine
```

The ready-made camera (`camera.hpp`) is:

```cpp
using Camera = projection::Camera<Mat4X4, ViewAngles, CameraTrait, NDCDepthRange::ZERO_TO_ONE>;
```

No `CameraAxes` override is passed, so `inverted_forward` and `inverted_right` stay `false`: the view matrix's third
row is the camera's forward axis (not its negation).

---

## Behavior

* **Look-at**:

  ```
  dir   = normalize(look_at - origin)
  pitch = asin(dir.z)
  yaw   = -atan2(dir.x, dir.y)
  roll  = 0
  ```

  Looking along `+Y` gives yaw 0, looking along `+X` gives yaw -90 degrees, looking along `-X` gives +90 degrees.

* **View matrix**: forwards to [`rage_engine::calc_view_matrix`](formulas.md#view-matrix).
* **Projection**: the `FieldOfView` you pass is **vertical**. It goes to
  [`calc_perspective_projection_matrix`](formulas.md#projection) together with `view_port.aspect_ratio()`.
* **`Camera::extract_projection_params`**: the trait has no `calc_field_of_view()`, so the generic camera uses its
  default path and returns the vertical FOV (`2 * atan(1 / m[1,1])`) plus the aspect ratio.
* **`Camera::calc_view_angles_from_view_matrix`**: the trait provides `extract_rotation_angles()`, so pitch, yaw **and**
  roll are recovered. At pitch +/-90 degrees roll is reported as 0 and the whole rotation goes into yaw.

---

## Example

```cpp
using namespace omath;

const rage_engine::Camera cam({0.f, 0.f, 1.7f}, {}, {1280.f, 720.f},
                              projection::FieldOfView::from_degrees(60.f), 0.01f, 1000.f);

// A camera with all angles at 0 looks along +Y
if (const auto screen = cam.world_to_screen({0.f, 10.f, 0.f}))
    draw_label(screen->x, screen->y);
```

The unit tests check that a point straight ahead (`{0, 10, 0}` from a camera at the origin, 1280x720 viewport) projects
to the exact center of the viewport, and that `calc_look_at_angle` toward `k_abs_forward`, `k_abs_right` and `k_abs_up`
produces angles whose `forward_vector` points at that axis.

---

## See also

* [Formulas](formulas.md)
* [Constants](constants.md) — axes, matrix and angle types
* [Camera](../../projection/camera.md) — the generic camera this trait plugs into
