# `omath::cry_engine` — types & constants

> Header: `omath/engines/cry_engine/constants.hpp`
> Namespace: `omath::cry_engine`
> Purpose: define the CryEngine coordinate system, matrix types, and angle ranges

---

## Summary

**CryEngine** (in omath) uses a **Z-up, Y-forward** coordinate system:

* **Up** = `{0, 0, 1}` (Z-axis)
* **Right** = `{1, 0, 0}` (X-axis)
* **Forward** = `{0, 1, 0}` (Y-axis)

The frame is right-handed in the mathematical sense (`right × forward = up`, i.e. `X × Y = Z`). Matrices are
**row-major**. One world unit is **one meter** (`units_to_meters` is the identity, see [Formulas](formulas.md#unit-conversions)).

The engine-space camera built on these axes uses a left-handed, vertical-FOV projection with a `ZERO_TO_ONE` depth
range, see [Camera Trait](camera_trait.md).

---

## Constants

```cpp
namespace omath::cry_engine {
  constexpr Vector3<float> k_abs_up      = {0, 0, 1};
  constexpr Vector3<float> k_abs_right   = {1, 0, 0};
  constexpr Vector3<float> k_abs_forward = {0, 1, 0};
}
```

---

## Matrix types

```cpp
using Mat4X4 = Mat<4, 4, float, MatStoreType::ROW_MAJOR>;
using Mat3X3 = Mat<3, 3, float, MatStoreType::ROW_MAJOR>;
using Mat1X3 = Mat<1, 3, float, MatStoreType::ROW_MAJOR>;
```

---

## Angle types

```cpp
using PitchAngle = Angle<float, -90.f, 90.f, AngleFlags::Clamped>;
using YawAngle   = Angle<float, -180.f, 180.f, AngleFlags::Normalized>;
using RollAngle  = Angle<float, -180.f, 180.f, AngleFlags::Normalized>;

using ViewAngles = omath::ViewAngles<PitchAngle, YawAngle, RollAngle>;
```

| Angle   | Range         | Out-of-range input | Positive direction |
|---------|---------------|--------------------|--------------------|
| `pitch` | -90 to 90     | clamped            | looks **up** (towards +Z) |
| `yaw`   | -180 to 180   | wrapped (270 becomes -90) | turns **left** (towards -X when looking down +Y) |
| `roll`  | -180 to 180   | wrapped            | around the camera's own forward (+Y) axis |

`ViewAngles` stores `{pitch, yaw, roll}` in that order and `as_vector3()` returns them as `{x = pitch, y = yaw, z = roll}`
in degrees.

---

## Coordinate system notes

* **Z-up**: gravity points along `-Z`, height increases with `+Z`
* **Forward = +Y** for a camera with all angles at 0; `right_vector({})` is `+X` and `up_vector({})` is `+Z`
* Horizontal plane is `X/Y`

---

## See also

* [Formulas](formulas.md) — rotation, view and projection matrices
* [Camera Trait](camera_trait.md) — plug-in for `projection::Camera`
* [View Angles](../../trigonometry/view_angles.md)
