# `omath::frostbite_engine` — types & constants

> Header: `omath/engines/frostbite_engine/constants.hpp`
> Namespace: `omath::frostbite_engine`
> Purpose: define the Frostbite (EA — Battlefield, etc.) coordinate system, matrix types, and angle ranges

---

## Summary

The **Frostbite Engine** uses a **Y-up, left-handed** coordinate system (the same axes as Unity):

* **Up** = `{0, 1, 0}` (Y-axis)
* **Right** = `{1, 0, 0}` (X-axis)
* **Forward** = `{0, 0, 1}` (Z-axis)

Matrices are **row-major**. Angles are **clamped pitch** (±90°) and **normalized yaw/roll** (±180°).

---

## Constants

```cpp
namespace omath::frostbite_engine {
  constexpr Vector3<float> k_abs_up      = {0, 1, 0};
  constexpr Vector3<float> k_abs_right   = {1, 0, 0};
  constexpr Vector3<float> k_abs_forward = {0, 0, 1};
}
```

These basis vectors define the engine's **world coordinate frame**.

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

* **PitchAngle**: clamped to **[-90°, +90°]**
* **YawAngle**: normalized to **[-180°, +180°]**
* **RollAngle**: normalized to **[-180°, +180°]**, around the camera's own forward axis

---

## Coordinate system notes

* **Y-up**: gravity points along `-Y`, height increases with `+Y`
* **Left-handed**: `right × up = forward` (forward is `+Z`)
* One world unit is one meter

---

## See also

* [Formulas](formulas.md) — direction vectors, rotation, view and projection matrices
* [Camera Trait](camera_trait.md) — plug-in for `projection::Camera`
