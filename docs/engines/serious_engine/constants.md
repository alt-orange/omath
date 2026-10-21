# `omath::serious_engine` — types & constants

> Header: `omath/engines/serious_engine/constants.hpp`
> Namespace: `omath::serious_engine`
> Purpose: define the Serious Engine (Croteam — Serious Sam) coordinate system, matrix types, and angle ranges

---

## Summary

**Serious Engine** uses a **Y-up, right-handed** coordinate system, the same axes as OpenGL:

* **Up** = `{0, 1, 0}` (Y-axis)
* **Right** = `{1, 0, 0}` (X-axis)
* **Forward** = `{0, 0, -1}` (negative Z-axis)

One world unit is **one meter**. Matrices are **row-major**, like the engine's own `FLOATmatrix3D`.

---

## Constants

```cpp
namespace omath::serious_engine {
  constexpr Vector3<float> k_abs_up      = {0, 1, 0};
  constexpr Vector3<float> k_abs_right   = {1, 0, 0};
  constexpr Vector3<float> k_abs_forward = {0, 0, -1};
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

These map onto the engine's `ANGLE3D` (heading, pitch, banking), in degrees:

| omath   | `ANGLE3D` | Positive direction       |
|---------|-----------|--------------------------|
| `yaw`   | heading   | turns **left** (towards -X when looking down -Z) |
| `pitch` | pitch     | looks **up**             |
| `roll`  | banking   | around the camera's own forward axis |

Note that `ANGLE3D` stores heading first; `ViewAngles` stores pitch first.

---

## Coordinate system notes

* **Y-up**: gravity points along `-Y`, height increases with `+Y`
* **Right-handed**: `right × up = -forward`
* **Forward = -Z** for a camera with all angles at 0

---

## See also

* [Formulas](formulas.md) — rotation, view and projection matrices
* [Camera Trait](camera_trait.md) — plug-in for `projection::Camera`
