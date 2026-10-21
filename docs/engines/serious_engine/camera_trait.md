# `omath::serious_engine::CameraTrait` — plug-in trait for `projection::Camera`

> Header: `omath/engines/serious_engine/traits/camera_trait.hpp`
> Namespace: `omath::serious_engine`
> Purpose: Serious Engine **look-at**, **view**, and **projection** math for the generic `omath::projection::Camera`

---

## API

```cpp
namespace omath::serious_engine {

class CameraTrait final {
public:
  static ViewAngles calc_look_at_angle(const Vector3<float>& cam_origin,
                                       const Vector3<float>& look_at) noexcept;

  static Mat4X4 calc_view_matrix(const ViewAngles& angles,
                                 const Vector3<float>& cam_origin) noexcept;

  static Mat4X4 calc_projection_matrix(const projection::FieldOfView& fov, // horizontal
                                       const projection::ViewPort& view_port,
                                       float near, float far,
                                       NDCDepthRange ndc_depth_range) noexcept;

  // Used by Camera::calc_view_angles_from_view_matrix(): inverse of rotation_matrix()
  static ViewAngles extract_rotation_angles(const Mat4X4& rotation) noexcept;

  // Used by Camera::extract_projection_params(): recovers the horizontal FOV
  static projection::FieldOfView calc_field_of_view(const Mat4X4& projection_matrix) noexcept;
};

} // namespace omath::serious_engine
```

The ready-made camera is:

```cpp
using Camera = projection::Camera<Mat4X4, ViewAngles, CameraTrait, NDCDepthRange::ZERO_TO_ONE,
                                  {.inverted_forward = true}>;
```

`inverted_forward` is set because the view matrix's third row points backwards (+Z for a level camera).

---

## Behavior

* **Look-at**:

  ```
  dir   = normalize(look_at - origin)
  pitch = asin(dir.y)
  yaw   = -atan2(dir.x, -dir.z)
  roll  = 0
  ```

* **Projection**: the `FieldOfView` you pass is **horizontal**, like the engine's own FOV setting.
* **`Camera::extract_projection_params`** returns that horizontal FOV, so it can be passed back to `set_field_of_view`.
* **`Camera::calc_view_angles_from_view_matrix`** recovers pitch, yaw and roll.

---

## Example

```cpp
using namespace omath;

const serious_engine::Camera cam({0.f, 1.7f, 0.f}, {}, {1280.f, 1024.f},
                                 projection::FieldOfView::from_degrees(90.f), 0.2f, 10000.f);

if (const auto screen = cam.world_to_screen({3.f, 1.f, -10.f}))
    draw_label(screen->x, screen->y);
```

---

## See also

* [Formulas](formulas.md)
* [Camera](../../projection/camera.md) — the generic camera this trait plugs into
