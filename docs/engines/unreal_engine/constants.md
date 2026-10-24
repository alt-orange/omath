# `omath::unreal_engine` — types & constants

> Header: `omath/engines/unreal_engine/constants.hpp`
> Namespace: `omath::unreal_engine`
> Purpose: define Unreal Engine coordinate system, matrix types, and angle ranges

---

## Summary

The **Unreal Engine** uses a **Z-up, left-handed** coordinate system:

* **Up** = `{0, 0, 1}` (Z-axis)
* **Right** = `{0, 1, 0}` (Y-axis)
* **Forward** = `{1, 0, 0}` (X-axis)

Matrices are **row-major**. Angles are **clamped pitch** (±90°) and **normalized yaw/roll** (±180°).

---

## Constants

```cpp
namespace omath::unreal_engine {
  constexpr Vector3<double> k_abs_up      = {0, 0, 1};
  constexpr Vector3<double> k_abs_right   = {0, 1, 0};
  constexpr Vector3<double> k_abs_forward = {1, 0, 0};
}
```

These basis vectors define the engine's **world coordinate frame**.

---

## Matrix types

```cpp
using Mat4X4 = Mat<4, 4, double, MatStoreType::ROW_MAJOR>;
using Mat3X3 = Mat<3, 3, double, MatStoreType::ROW_MAJOR>;
using Mat1X3 = Mat<1, 3, double, MatStoreType::ROW_MAJOR>;
```

Unreal Engine 5 uses double-precision world coordinates, so every type in this engine namespace is `double`-based.

**Row-major** storage means rows are contiguous in memory. Suitable for CPU-side transforms and typical C++ math libraries.

---

## Angle types

```cpp
using PitchAngle = Angle<double, -90., 90., AngleFlags::Clamped>;
using YawAngle   = Angle<double, -180., 180., AngleFlags::Normalized>;
using RollAngle  = Angle<double, -180., 180., AngleFlags::Normalized>;

using ViewAngles = omath::ViewAngles<PitchAngle, YawAngle, RollAngle>;
```

* **PitchAngle**: clamped to **[-90°, +90°]** (positive pitch looks **up**, negative looks down)
* **YawAngle**: normalized to **[-180°, +180°]** (horizontal rotation)
* **RollAngle**: normalized to **[-180°, +180°]** (camera roll)

`ViewAngles` bundles all three into a single type for camera/view transforms.

---

## Coordinate system notes

* **Z-up**: gravity points along `-Z`, height increases with `+Z`
* **Left-handed**: cross product `forward × right = up` with left-hand rule
* This matches **Unreal Engine** conventions for 3D games and simulations

---

## See also

* `omath/engines/unreal_engine/formulas.hpp` — view/projection matrix builders
* `omath/trigonometry/angle.hpp` — angle normalization & clamping helpers
* `omath/trigonometry/view_angles.hpp` — generic pitch/yaw/roll wrapper
