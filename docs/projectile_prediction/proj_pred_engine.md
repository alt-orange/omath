# `omath::projectile_prediction::ProjPredEngineInterface` — aim solver contract and result types

> Header: `omath/projectile_prediction/proj_pred_engine.hpp`
> Namespace: `omath::projectile_prediction`
> Template: `ProjPredEngineInterface<ArithmeticType = float>`
> Depends on: [`Vector3`](../linear_algebra/vector3.md), [`Projectile`](projectile.md), [`Launcher`](launcher.md), [`Target`](target.md)
> Purpose: the abstract base of every aim solver, plus the `AimAngles`, `Arc` and `AimSolution` types they return

For a longer walk-through, implementation guidance and a closed-form example, see [`projectile_engine.md`](projectile_engine.md). This page is the reference for the header's declarations.

---

## API

```cpp
namespace omath::projectile_prediction {

// View angles in degrees. Pitch is positive upwards whatever the engine's own sign convention is.
template<class ArithmeticType = float>
struct AimAngles { ArithmeticType pitch{}; ArithmeticType yaw{}; };

// Which of the two pitches that reach a point an engine solves for.
enum class Arc { LOW, HIGH };

template<class ArithmeticType = float>
struct AimSolution final {
  AimAngles<ArithmeticType> angles;              // view angles to set on the eye
  Vector3<ArithmeticType>   aim_point;           // point on the aim ray, as far away as the predicted target
  Vector3<ArithmeticType>   predicted_target_position;
  ArithmeticType            time_of_flight{};
};

template<class ArithmeticType = float>
class ProjPredEngineInterface {
public:
  [[nodiscard]]
  virtual std::optional<AimSolution<ArithmeticType>>
  maybe_calculate_aim(const Projectile<ArithmeticType>& projectile,
                      const Launcher<ArithmeticType>& launcher,
                      const Target<ArithmeticType>& target) const = 0;

  // Compatibility wrappers over maybe_calculate_aim()
  [[nodiscard]] std::optional<Vector3<ArithmeticType>>
  maybe_calculate_aim_point(const Projectile<ArithmeticType>&, const Target<ArithmeticType>&) const;

  [[nodiscard]] std::optional<AimAngles<ArithmeticType>>
  maybe_calculate_aim_angles(const Projectile<ArithmeticType>&, const Target<ArithmeticType>&) const;

  [[nodiscard]] static constexpr Launcher<ArithmeticType>
  launcher_from_projectile(const Projectile<ArithmeticType>& projectile) noexcept;

  virtual ~ProjPredEngineInterface() = default;

protected:
  ProjPredEngineInterface() = default;                                         // plus protected copy / move
};

} // namespace omath::projectile_prediction
```

---

## Behavior

* **`maybe_calculate_aim`** is the only pure virtual. It returns an `AimSolution`, or `std::nullopt` when the engine finds no intercept.
* **`AimSolution::angles`** are the view angles to put on the eye. The round leaves along them raised by `Launcher::launch_pitch_offset`, so the launch pitch is `angles.pitch + launcher.launch_pitch_offset`.
* **`AimSolution::aim_point`** lies on the eye's aim ray at the distance of the predicted target. Every point on that ray projects to the same pixel, so this is where a crosshair or marker belongs on screen.
* **`Arc`**: with gravity there are two pitches that reach a point, a flat quick one (`LOW`) and a steep lob (`HIGH`). `LOW` is the default for the engines that take it as a constructor argument ([legacy](proj_pred_engine_legacy.md) and [drag](proj_pred_engine_drag.md)). A round without gravity has one straight line and the choice does not apply.
* **Compatibility wrappers.** `maybe_calculate_aim_point` and `maybe_calculate_aim_angles` build a `Launcher` with `launcher_from_projectile` (eye = `projectile.m_origin`, `world_offset = projectile.m_launch_offset`, no muzzle offset, no launch pitch offset), call `maybe_calculate_aim`, and return one field of the solution. The header marks them as kept for one release; new code should call `maybe_calculate_aim` with a `Launcher`.
* **Copy and move are `protected`** on the base, so a derived engine stays copyable but nothing can be sliced through a base reference.

---

## Implementations

| Class | Page | Notes |
|---|---|---|
| `ProjPredEngineLegacy` | [legacy](proj_pred_engine_legacy.md) | Trait-based fixed-step scan with a closed-form pitch. Kept for backward compatibility; always available. |
| `ProjPredEngineAvx2` | [avx2](proj_pred_engine_avx2.md) | Vectorised `float` engine for x86. |
| `ProjPredEngineDrag` | [drag](proj_pred_engine_drag.md) | Rounds with air drag; simulates the flight. |

The trait-based engines are parameterized on an engine trait that satisfies [`PredEngineConcept`](pred_engine_concept.md).

---

## Usage

```cpp
#include <omath/projectile_prediction/proj_pred_engine.hpp>

using namespace omath::projectile_prediction;

void aim_at(const ProjPredEngineInterface<float>& engine, const Target<float>& target)
{
    const Projectile<float> projectile{.m_launch_speed = 850.f, .m_gravity_scale = 1.f};
    const Launcher<float> launcher{.eye_origin = {0.f, 0.f, 64.f}};

    if (const auto solution = engine.maybe_calculate_aim(projectile, launcher, target))
    {
        set_view_angles(solution->angles.pitch, solution->angles.yaw);
        draw_marker(solution->aim_point);
    }
    // std::nullopt: target outruns the round, is out of range, or the input is degenerate
}
```

---

## Notes

* The wrappers each run the full solve; a caller that wants both the point and the angles should call `maybe_calculate_aim` once.
* The interface methods are `const`, so one engine instance can serve many queries.
* Copying through a base reference is not possible by design; hold engines by concrete type, by `std::unique_ptr<ProjPredEngineInterface<T>>`, or by reference.

---

## See also

* [Projectile engine overview](projectile_engine.md)
* [`PredEngineConcept`](pred_engine_concept.md)
* [Launcher](launcher.md), [Projectile](projectile.md), [Target](target.md)
* [Projectile flight](projectile_flight.md)
