# `omath::projectile_prediction::PredEngineConcept` — what an engine trait must provide

> Header: `omath/projectile_prediction/pred_engine_concept.hpp`
> Namespace: `omath::projectile_prediction`
> Depends on: [`Vector3`](../linear_algebra/vector3.md), [`Projectile`](projectile.md), [`Target`](target.md), `ViewBasis` (declared in [`launcher.hpp`](launcher.md))
> Purpose: compile-time contract for the per-engine `PredEngineTrait` classes that the prediction engines are parameterized on

---

## API

```cpp
namespace omath::projectile_prediction {

template<class T, class ArithmeticType>
concept PredEngineConcept =
    requires(const Projectile<ArithmeticType>& projectile, const Target<ArithmeticType>& target,
             const Vector3<ArithmeticType>& vec_a, const Vector3<ArithmeticType>& vec_b,
             ArithmeticType pitch, ArithmeticType yaw, ArithmeticType time, ArithmeticType gravity) {
      { T::predict_projectile_position(vec_a, projectile, pitch, yaw, time, gravity) } -> std::same_as<Vector3<ArithmeticType>>;
      { T::predict_target_position(target, time, gravity) }                           -> std::same_as<Vector3<ArithmeticType>>;
      { T::calc_vector_2d_distance(vec_a) }                                           -> std::same_as<ArithmeticType>;
      { T::get_vector_height_coordinate(vec_b) }                                      -> std::same_as<ArithmeticType>;
      { T::calc_view_basis(pitch, yaw) }                                              -> std::same_as<ViewBasis<ArithmeticType>>;
      { T::calc_direct_pitch_angle(vec_a, vec_b) }                                    -> std::same_as<ArithmeticType>;
      { T::calc_direct_yaw_angle(vec_a, vec_b) }                                      -> std::same_as<ArithmeticType>;

      // every one of the calls above must also be noexcept
    };

} // namespace omath::projectile_prediction
```

---

## Required members

All seven are **static** functions of `T`, callable as `T::name(...)`, with exactly the return types shown and `noexcept`.

| Function | Meaning |
|---|---|
| `predict_projectile_position(launch_origin, projectile, pitch, yaw, time, gravity)` | Where the round is `time` seconds after leaving `launch_origin` at launch `pitch` / `yaw`, under gravity. |
| `predict_target_position(target, time, gravity)` | Where the [`Target`](target.md) is after `time` seconds (the Source trait: linear motion, plus a fall under gravity if the target is airborne). |
| `calc_vector_2d_distance(delta)` | Length of a vector in the engine's ground plane. |
| `get_vector_height_coordinate(vec)` | The component of a vector along the engine's up axis (`z` for a Z-up engine, `y` for a Y-up engine). |
| `calc_view_basis(pitch, yaw)` | `ViewBasis{forward, right, up}` of a view with those angles, in the engine's world axes. |
| `calc_direct_pitch_angle(origin, view_to)` | Pitch in degrees of the straight line from `origin` to `view_to`. |
| `calc_direct_yaw_angle(origin, view_to)` | Yaw in degrees of the straight line from `origin` to `view_to`. |

Angles are in degrees. The prediction engines hand the traits a pitch that is positive when looking **up**, so a trait whose engine uses the opposite sign converts internally (the Source trait negates the pitch before building the engine's angle type).

---

## Where the concept is used

Constraining the `EngineTrait` template parameter of:

* [`ProjPredEngineLegacy`](proj_pred_engine_legacy.md)
* [`ProjPredEngineDrag`](proj_pred_engine_drag.md)
* `ProjectileFlight` (see [Projectile flight](projectile_flight.md))

Every supported engine ships a trait that satisfies it, for example [Source](../engines/source_engine/pred_engine_trait.md), [Unreal](../engines/unreal_engine/pred_engine_trait.md), [Unity](../engines/unity_engine/pred_engine_trait.md), [IW](../engines/iw_engine/pred_engine_trait.md), [Frostbite](../engines/frostbite/pred_engine_trait.md), [OpenGL](../engines/opengl_engine/pred_engine_trait.md) and [Serious](../engines/serious_engine/pred_engine_trait.md).

---

## Usage

Checking a trait at compile time:

```cpp
#include <omath/projectile_prediction/pred_engine_concept.hpp>
#include <omath/engines/source_engine/traits/pred_engine_trait.hpp>

static_assert(omath::projectile_prediction::PredEngineConcept<omath::source_engine::PredEngineTrait, float>);
```

Writing a generic function over any engine trait:

```cpp
template<class EngineTrait>
requires omath::projectile_prediction::PredEngineConcept<EngineTrait, float>
float horizontal_range(const omath::Vector3<float>& from, const omath::Vector3<float>& to)
{
    return EngineTrait::calc_vector_2d_distance(to - from);
}
```

A skeleton for a new engine:

```cpp
struct MyEnginePredTrait final {
  static omath::Vector3<float> predict_projectile_position(const omath::Vector3<float>& launch_origin,
      const omath::projectile_prediction::Projectile<float>& projectile,
      float pitch, float yaw, float time, float gravity) noexcept;
  static omath::Vector3<float> predict_target_position(const omath::projectile_prediction::Target<float>& target,
      float time, float gravity) noexcept;
  static float calc_vector_2d_distance(const omath::Vector3<float>& delta) noexcept;
  static float get_vector_height_coordinate(const omath::Vector3<float>& vec) noexcept;
  static omath::projectile_prediction::ViewBasis<float> calc_view_basis(float pitch, float yaw) noexcept;
  static float calc_direct_pitch_angle(const omath::Vector3<float>& origin, const omath::Vector3<float>& view_to) noexcept;
  static float calc_direct_yaw_angle(const omath::Vector3<float>& origin, const omath::Vector3<float>& view_to) noexcept;
};
```

---

## Notes

* The concept checks return types and `noexcept`, not behavior. A trait with a wrong sign convention still satisfies it; the existing traits are exercised by `tests/engines/unit_test_pred_engine_traits_all.cpp`.
* `ArithmeticType` is part of the concept, so a trait written for `float` does not satisfy `PredEngineConcept<T, double>`.
* Return types are checked with `std::same_as`: a function returning `double` where `float` is required does not match.

---

## See also

* [`ProjPredEngineInterface`](proj_pred_engine.md)
* [Launcher](launcher.md), [Projectile](projectile.md), [Target](target.md)
* [Source engine prediction trait](../engines/source_engine/pred_engine_trait.md)
