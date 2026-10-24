# `omath::Quaternion` — unit quaternion rotation

> Header: `omath/linear_algebra/quaternion.hpp`
> Namespace: `omath`
> Template: `Quaternion<Type>` with `std::is_arithmetic_v<Type>`
> Depends on: [`Vector3`](vector3.md), [`Mat`](mat.md)
> Purpose: represent a 3D rotation as `(x, y, z, w)`, compose rotations, rotate vectors, and convert to a rotation matrix

---

## API

```cpp
namespace omath {

template<class Type>
requires std::is_arithmetic_v<Type>
class Quaternion {
public:
  using ContainedType = Type;

  Type x = 0, y = 0, z = 0;
  Type w = 1;                                   // default is the identity rotation

  constexpr Quaternion() noexcept = default;
  constexpr Quaternion(const Type& x, const Type& y, const Type& z, const Type& w) noexcept;

  // axis must already be normalized; angle in radians
  [[nodiscard]] static Quaternion from_axis_angle(const Vector3<Type>& axis, const Type& angle_rad) noexcept;

  [[nodiscard]] constexpr bool operator==(const Quaternion&) const noexcept;
  [[nodiscard]] constexpr bool operator!=(const Quaternion&) const noexcept;

  [[nodiscard]] constexpr Quaternion operator*(const Quaternion& other) const noexcept;  // Hamilton product
  constexpr Quaternion& operator*=(const Quaternion& other) noexcept;
  [[nodiscard]] constexpr Quaternion operator*(const Type& scalar) const noexcept;
  constexpr Quaternion& operator*=(const Type& scalar) noexcept;
  [[nodiscard]] constexpr Quaternion operator+(const Quaternion& other) const noexcept;
  constexpr Quaternion& operator+=(const Quaternion& other) noexcept;
  [[nodiscard]] constexpr Quaternion operator-() const noexcept;

  [[nodiscard]] constexpr Quaternion conjugate() const noexcept;
  [[nodiscard]] constexpr Type dot(const Quaternion& other) const noexcept;
  [[nodiscard]] constexpr Type length_sqr() const noexcept;
  [[nodiscard]] constexpr Type length() const noexcept;          // not constexpr on MSVC
  [[nodiscard]] constexpr Quaternion normalized() const noexcept; // not constexpr on MSVC
  [[nodiscard]] constexpr Quaternion inverse() const noexcept;

  [[nodiscard]] constexpr Vector3<Type> rotate(const Vector3<Type>& v) const noexcept;
  [[nodiscard]] constexpr Mat<3, 3, Type> to_rotation_matrix3() const noexcept;
  [[nodiscard]] constexpr Mat<4, 4, Type> to_rotation_matrix4() const noexcept;
  [[nodiscard]] constexpr std::array<Type, 4> as_array() const noexcept;   // {x, y, z, w}
};

} // namespace omath

// std::formatter<omath::Quaternion<Type>> prints "[x, y, z, w]" (char, wchar_t, char8_t)
```

---

## Behavior

* **Layout and identity**: the vector part is `(x, y, z)` and the scalar part is `w`. A default-constructed quaternion is the identity `(0, 0, 0, 1)`.
* **`from_axis_angle(axis, angle_rad)`**: returns `(axis * sin(angle/2), cos(angle/2))`. The axis is **not** normalized for you; pass a unit vector, otherwise the result is not a unit quaternion.
* **`operator*`** is the Hamilton product `this * other`. It is not commutative; the result applies `other` first and then `this` when used to rotate a vector.
* **`conjugate()`** negates `x, y, z` and keeps `w`.
* **`inverse()`** is `conjugate() / length_sqr()`. For a unit quaternion it equals the conjugate. It divides by `length_sqr()` with no zero check, so do not invert the all-zero quaternion.
* **`normalized()`** returns `*this` unchanged when `length()` is zero instead of dividing by zero.
* **`length()` / `normalized()`** use `std::sqrt`, so they are `constexpr` only on non-MSVC toolchains (the header splits them with `#ifndef _MSC_VER`).
* **`rotate(v)`** computes `q * v * q^-1` through Rodrigues' formula (two cross products), which assumes a **unit** quaternion.
* **`to_rotation_matrix3()` / `to_rotation_matrix4()`** build the standard column-vector rotation matrix of a unit quaternion. The 4x4 variant has a zero translation column/row and `1` in the corner. Normalize first if the quaternion has drifted.
* **`operator+`, `operator*(scalar)`, unary `-`** are component-wise and do **not** keep the result normalized.
* **`as_array()`** returns `{x, y, z, w}`.

---

## Usage

```cpp
#include <omath/linear_algebra/quaternion.hpp>
#include <numbers>

using omath::Quaternion;
using omath::Vector3;

// Rotate +X by 90 degrees around +Z -> +Y
const auto q = Quaternion<float>::from_axis_angle({0.f, 0.f, 1.f}, std::numbers::pi_v<float> / 2.f);
const Vector3<float> r = q.rotate({1.f, 0.f, 0.f});   // ~(0, 1, 0)

// Compose: rotate around Z first, then around X
const auto qx = Quaternion<float>::from_axis_angle({1.f, 0.f, 0.f}, std::numbers::pi_v<float> / 2.f);
const auto combined = qx * q;

// Undo a rotation
const auto back = q.inverse().rotate(r);               // ~(1, 0, 0)

// Matrix form for pipelines that want a Mat
const auto m3 = q.to_rotation_matrix3();
const auto m4 = q.to_rotation_matrix4();
```

---

## Notes

* Equality (`==`) compares the four floats exactly. Compare rotations with a tolerance (for example on `dot()`), as the unit tests do.
* `q` and `-q` describe the same rotation but are not `==`.
* The class is exposed to Lua as `omath.Quaternion`; see [Lua bindings](../lua/lua.md).

---

## See also

* [`Vector3`](vector3.md)
* [`Mat`](mat.md)
* [Angle types](../trigonometry/angle.md)
