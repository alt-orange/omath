# `omath::ViewAngles` — tiny POD for pitch/yaw/roll

> Header: `omath/trigonometry/view_angles.hpp`
> Namespace: `omath`
> Kind: **aggregate struct**, one helper method (`as_vector3()`), no allocation
> Requires: all three component types expose the same `ArithmeticType` (as `omath::Angle<>` does)

A minimal container for Euler angles. You choose the component types (normally `omath::Angle<>` specializations, which provide the required `ArithmeticType` member), and plug it into systems like `projection::Camera`.

---

## API

```cpp
namespace omath {
  template<class PitchType, class YawType, class RollType>
  requires std::is_same_v<typename PitchType::ArithmeticType, typename YawType::ArithmeticType>
        && std::is_same_v<typename YawType::ArithmeticType,  typename RollType::ArithmeticType>
  struct ViewAngles {
    using ArithmeticType = PitchType::ArithmeticType;

    PitchType pitch;
    YawType   yaw;
    RollType  roll;

    // {pitch, yaw, roll} in degrees
    constexpr Vector3<ArithmeticType> as_vector3() const;
  };
}
```

* Aggregate: supports brace-init, aggregate copying, and `constexpr` usage when the component types do.
* Component types must define `ArithmeticType`; plain `float` does **not** satisfy the constraint, so `ViewAngles<float, float, float>` does not compile.
* Semantics (units/handedness/ranges) are **entirely defined by your chosen types**.

---

## Common aliases

```cpp
// Policy-based angles
using PitchDeg = omath::Angle<float, -89.f,  89.f,  omath::AngleFlags::Clamped>;
using YawDeg   = omath::Angle<float, -180.f, 180.f, omath::AngleFlags::Normalized>;
using RollDeg  = omath::Angle<float, -180.f, 180.f, omath::AngleFlags::Normalized>;
using ViewAnglesDeg = omath::ViewAngles<PitchDeg, YawDeg, RollDeg>;
```

---

## Examples

### Reading back as a vector

```cpp
ViewAnglesDeg v{ PitchDeg::from_degrees(10.f), YawDeg::from_degrees(45.f), RollDeg::from_degrees(0.f) };
omath::Vector3<float> deg = v.as_vector3();   // (10, 45, 0): pitch, yaw, roll in degrees
```

### With `omath::Angle<>` (automatic wrap/clamper)

```cpp
ViewAnglesDeg v{
  PitchDeg::from_degrees(  95.f), // ->  89deg (clamped)
  YawDeg::from_degrees   (-190.f), // -> 170deg (wrapped)
  RollDeg::from_degrees  (  30.f)
};
```

### Using with `projection::Camera`

```cpp
using Mat4 = omath::Mat<4,4,float, omath::MatStoreType::COLUMN_MAJOR>;
using Cam  = omath::projection::Camera<Mat4, ViewAnglesDeg, MyCameraTrait>;

omath::projection::ViewPort vp{1920,1080};
auto fov = omath::angles::degrees_to_radians(70.f); // or your Angle type

Cam cam(/*position*/ {0,1.7f,-3},
        /*angles*/   ViewAnglesDeg{ PitchDeg::from_degrees(0),
                                    YawDeg::from_degrees(0),
                                    RollDeg::from_degrees(0) },
        /*viewport*/ vp,
        /*fov*/      omath::Angle<float,0.f,180.f,omath::AngleFlags::Clamped>::from_degrees(70.f),
        /*near*/     0.1f,
        /*far*/      1000.f);
```

---

## Notes & tips

* **Ranges/units**: pick types that encode your policy (e.g., signed yaw in `[-180,180]`, pitch clamped to avoid gimbal flips).
* **Handedness & order**: this struct doesn’t impose rotation order. Your math/trait layer (e.g., `MyCameraTrait`) must define how `(pitch, yaw, roll)` map to a view matrix (common orders: ZYX or XYZ).
* **Zero-cost**: it holds three `Angle<>` values; the wrap/clamp logic runs only on construction/arithmetic of those angles.
