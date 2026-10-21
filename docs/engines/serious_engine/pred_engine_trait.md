# `omath::serious_engine::PredEngineTrait` — projectile prediction trait

> Header: `omath/engines/serious_engine/traits/pred_engine_trait.hpp`
> Namespace: `omath::serious_engine`
> Purpose: Serious Engine axes for the projectile prediction engines

---

## Summary

`PredEngineTrait` adapts the generic projectile solvers to Serious Engine's **Y-up, -Z forward** world:

* **Height** is `y`; gravity is subtracted from `y`.
* **Horizontal distance** uses `x` and `z`.
* Launch directions come from `forward_vector({pitch, yaw, 0})`, so pitch/yaw use the engine's signs:
  positive pitch looks up, positive yaw (heading) turns left.

---

## API

```cpp
namespace omath::serious_engine {

class PredEngineTrait final {
public:
  static Vector3<float> predict_projectile_position(const Vector3<float>& launch_origin,
                                                    const projectile_prediction::Projectile<float>& projectile,
                                                    float pitch, float yaw, float time, float gravity) noexcept;

  static Vector3<float> predict_target_position(const projectile_prediction::Target<float>& target,
                                                float time, float gravity) noexcept;

  static float calc_vector_2d_distance(const Vector3<float>& delta) noexcept;      // sqrt(x² + z²)
  static float get_vector_height_coordinate(const Vector3<float>& vec) noexcept;   // y

  static projectile_prediction::ViewBasis<float> calc_view_basis(float pitch, float yaw) noexcept;

  static float calc_direct_pitch_angle(const Vector3<float>& origin, const Vector3<float>& view_to) noexcept;
  static float calc_direct_yaw_angle(const Vector3<float>& origin, const Vector3<float>& view_to) noexcept;
};

} // namespace omath::serious_engine
```

Angles are in **degrees**:

```
pitch = asin(dir.y)
yaw   = -atan2(dir.x, -dir.z)
```

---

## See also

* [Formulas](formulas.md)
* [Projectile prediction](../../projectile_prediction/projectile_engine.md)
