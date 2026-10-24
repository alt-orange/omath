# `omath::cry_engine::PredEngineTrait` — projectile prediction trait

> Header: `omath/engines/cry_engine/traits/pred_engine_trait.hpp`
> Namespace: `omath::cry_engine`
> Purpose: CryEngine axes for the projectile prediction engines

---

## Summary

`PredEngineTrait` adapts the generic projectile solvers to CryEngine's **Z-up, +Y forward** world:

* **Height** is `z`; gravity is subtracted from `z`.
* **Horizontal distance** uses `x` and `y`.
* Launch directions come from `forward_vector({pitch, yaw, 0})`, so pitch/yaw use the engine's signs: positive pitch
  looks up, positive yaw turns left (towards `-X` when looking down `+Y`).
* Angles passed in and returned are in **degrees**.

---

## API

```cpp
namespace omath::cry_engine {

class PredEngineTrait final {
public:
  static constexpr Vector3<float>
  predict_projectile_position(const Vector3<float>& launch_origin,
                              const projectile_prediction::Projectile<float>& projectile,
                              float pitch, float yaw, float time, float gravity) noexcept;

  static constexpr Vector3<float>
  predict_target_position(const projectile_prediction::Target<float>& target,
                          float time, float gravity) noexcept;

  static float calc_vector_2d_distance(const Vector3<float>& delta) noexcept;          // sqrt(x² + y²)
  static constexpr float get_vector_height_coordinate(const Vector3<float>& vec) noexcept; // z

  static constexpr projectile_prediction::ViewBasis<float> calc_view_basis(float pitch, float yaw) noexcept;

  static float calc_direct_pitch_angle(const Vector3<float>& origin, const Vector3<float>& view_to) noexcept;
  static float calc_direct_yaw_angle(const Vector3<float>& origin, const Vector3<float>& view_to) noexcept;
};

} // namespace omath::cry_engine
```

---

## Behavior

* **`predict_projectile_position`** — `launch_origin` is used as given (add `projectile.m_launch_offset` yourself):

  ```
  pos   = launch_origin + forward_vector({pitch, yaw, 0}) * projectile.m_launch_speed * time
  pos.z -= gravity * projectile.m_gravity_scale * time² / 2
  ```

* **`predict_target_position`**:

  ```
  pos = target.m_origin + target.m_velocity * time
  if target.m_is_airborne:  pos.z -= gravity * time² / 2
  ```

  Gravity is applied to the target only while it is airborne, and `m_gravity_scale` does not apply to targets.

* **`calc_view_basis`** — `forward`, `right` and `up` are `k_abs_forward`, `k_abs_right` and `k_abs_up` rotated by
  [`rotation_matrix`](formulas.md#rotation-matrix) built from `{pitch, yaw, roll = 0}`.

* **Direct angles** (in degrees), with `dir = normalize(view_to - origin)`:

  ```
  pitch = asin(dir.z)
  yaw   = -atan2(dir.x, dir.y)
  ```

  So pitch is +90 for a target straight up and -90 straight down; yaw is 0 for a target along `+Y`, -90 along `+X`,
  +90 along `-X`, and +/-180 along `-Y`.

  A comment in the header notes that the launch-pitch solver expects pitch in this convention (positive looks up,
  negative looks down; roughly +/-89 degrees at the extremes).

---

## Example

```cpp
using namespace omath;
using Trait = cry_engine::PredEngineTrait;

projectile_prediction::Projectile<float> projectile;
projectile.m_origin = {0.f, 0.f, 0.f};
projectile.m_launch_speed = 10.f;
projectile.m_gravity_scale = 1.f;

// Level shot straight down +Y, after 1 s with g = 9.81:
const auto pos = Trait::predict_projectile_position(projectile.m_origin + projectile.m_launch_offset,
                                                    projectile, 0.f, 0.f, 1.f, 9.81f);
// pos == {0, 10, -4.905}
```

---

## See also

* [Formulas](formulas.md) — `forward_vector`, `rotation_matrix`
* [Constants](constants.md) — `k_abs_forward` / `k_abs_right` / `k_abs_up`
* [Projectile prediction](../../projectile_prediction/projectile_engine.md)
* [Projectile](../../projectile_prediction/projectile.md), [Target](../../projectile_prediction/target.md)
