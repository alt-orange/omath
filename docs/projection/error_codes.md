# `omath::projection::Error` — Error codes for world/screen projection

> Header: `omath/projection/error_codes.hpp`
> Namespace: `omath::projection`
> Type: `enum class Error : uint16_t`

These error codes are returned by camera/projection helpers (e.g., `Camera::world_to_screen`, `Camera::screen_to_world`) wrapped in `std::expected<..., Error>`. Use them to distinguish **clipping/visibility** problems from **matrix/math** failures.

---

## Enum values

```cpp
enum class Error : uint16_t {
  WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS,
  INV_VIEW_PROJ_MAT_DET_EQ_ZERO,
  PERSPECTIVE_DIVIDER_LESS_EQ_ZERO,
};
```

* **`WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS`**
  The input point cannot produce a valid on-screen coordinate:

    * After projection, the NDC x/y is outside `[-1, 1]` or the NDC z is outside the engine's depth range (`[-1, 1]` or `[0, 1]`). The `*_unclipped` variants (`ViewPortClipping::MANUAL`) check only z.

* **`INV_VIEW_PROJ_MAT_DET_EQ_ZERO`**
  The **View × Projection** matrix is not invertible (determinant ≈ 0).
  Unprojection (`view_port_to_world` / `screen_to_world`) requires an invertible matrix.

* **`PERSPECTIVE_DIVIDER_LESS_EQ_ZERO`**
  The homogeneous `w` is unusable. When projecting, `w <= epsilon` means the point is at or behind the camera plane; when unprojecting, `|w| < epsilon` after multiplying by the inverse view-projection matrix.

---

## Typical usage

```cpp
using omath::projection::Error;

auto pix = cam.world_to_screen(point);
if (!pix) {
  switch (pix.error()) {
    case Error::WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS:
      // Cull label/marker; point is off-screen or behind camera.
      break;
    case Error::INV_VIEW_PROJ_MAT_DET_EQ_ZERO:
      // Investigate camera/projection setup; near/far/FOV or trait bug.
      break;
    case Error::PERSPECTIVE_DIVIDER_LESS_EQ_ZERO:
      // Point is at/behind the camera plane (or degenerate unprojection).
      break;
  }
}

// Unproject a screen pixel (top-left origin) at depth 1.0
if (auto world = cam.screen_to_world({sx, sy, 1.0f})) {
  // use *world
} else if (world.error() == Error::INV_VIEW_PROJ_MAT_DET_EQ_ZERO) {
  // handle singular VP matrix
}
```

---

## When you might see these errors

* **Out-of-bounds**

    * The world point lies outside the camera frustum.
    * Extremely large coordinates cause overflow and fail NDC bounds.

* **Non-positive perspective divider**

    * The point is behind the camera (clip `w <= 0`) or exactly on the camera plane.

* **Non-invertible VP**

    * Degenerate projection settings (e.g., `near == far`, zero FOV).
    * Trait builds `P` or `V` incorrectly (wrong handedness/order).
    * Numerical issues from nearly singular configurations.

---

## Recommendations

* Validate camera setup: `near > 0`, `far > near`, sensible FOV (e.g., 30°–120°).
* For UI markers: treat `WORLD_POSITION_IS_OUT_OF_SCREEN_BOUNDS` and `PERSPECTIVE_DIVIDER_LESS_EQ_ZERO` as simple **cull** signals.
* Log `INV_VIEW_PROJ_MAT_DET_EQ_ZERO` — it usually indicates a configuration or math bug worth fixing rather than hiding.
