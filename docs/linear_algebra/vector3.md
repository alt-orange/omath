# `omath::Vector3` — 3D vector (C++20/23)

> Header: `omath/linear_algebra/vector3.hpp`
> Namespace: `omath`
> Template: `template<class Type> requires std::is_arithmetic_v<Type>`
> Depends on: `omath::Vector2<Type>` (base class), `omath::Angle` (for `angle_between`)
> C++: uses `std::expected` ⇒ **C++23** recommended (or a backport)

`Vector3<Type>` extends `Vector2<Type>` with a `z` component and 3D operations: arithmetic, geometry (dot, cross, distance), normalization, angle-between with robust error signaling, hashing (generic `std::hash` specialization) and `std::formatter` support.

---

## Quick start

```cpp
#include <omath/linear_algebra/vector3.hpp>
using omath::Vector3;

using Vec3f = Vector3<float>;

Vec3f a{3, 4, 0};
Vec3f b{1, 2, 2};

auto d     = a.distance_to(b);         // Euclidean distance
auto dot   = a.dot(b);                 // 3*1 + 4*2 + 0*2 = 11
auto cr    = a.cross(b);               // (8, -6, 2)
auto len   = a.length();               // hypot(x,y,z)
auto unit  = a.normalized();           // safe normalize (returns a if length==0)

if (auto ang = a.angle_between(b)) {
  float deg = ang->as_degrees();       // [0, 180], clamped
} else {
  // vectors have zero length -> no defined angle
}
```

---

## Data members

```cpp
Type x{0};  // inherited from Vector2<Type>
Type y{0};  // inherited from Vector2<Type>
Type z{0};
```

---

## Constructors

```cpp
constexpr Vector3() noexcept;
constexpr Vector3(const Type& x, const Type& y, const Type& z) noexcept;

// explicit cast to another arithmetic component type
template<class CastedType> requires std::is_arithmetic_v<CastedType>
constexpr explicit operator Vector3<CastedType>() const noexcept;
```

`using ContainedType = Type;` is also declared.

---

## Equality & ordering

```cpp
constexpr bool operator==(const Vector3&) const noexcept; // component-wise
constexpr bool operator!=(const Vector3&) const noexcept;

constexpr bool operator< (const Vector3&) const noexcept; // compare by length_sqr()
constexpr bool operator> (const Vector3&) const noexcept;
constexpr bool operator<=(const Vector3&) const noexcept;
constexpr bool operator>=(const Vector3&) const noexcept;
```

> **Note:** Ordering uses **magnitude** (squared length), not lexicographic order.

---

## Arithmetic (mutating)

Component-wise with another vector:

```cpp
constexpr Vector3& operator+=(const Vector3&) noexcept;
constexpr Vector3& operator-=(const Vector3&) noexcept;
constexpr Vector3& operator*=(const Vector3&) noexcept;  // Hadamard product
constexpr Vector3& operator/=(const Vector3&) noexcept;
```

With a scalar:

```cpp
constexpr Vector3& operator*=(const Type& v) noexcept;
constexpr Vector3& operator/=(const Type& v) noexcept;
constexpr Vector3& operator+=(const Type& v) noexcept;
constexpr Vector3& operator-=(const Type& v) noexcept;
```

---

## Arithmetic (non-mutating)

```cpp
constexpr Vector3 operator-()                     const noexcept;
constexpr Vector3 operator+(const Vector3&)       const noexcept;
constexpr Vector3 operator-(const Vector3&)       const noexcept;
constexpr Vector3 operator*(const Vector3&)       const noexcept; // Hadamard
constexpr Vector3 operator/(const Vector3&)       const noexcept; // Hadamard
constexpr Vector3 operator*(const Type& scalar)   const noexcept;
constexpr Vector3 operator/(const Type& scalar)   const noexcept;
```

---

## Geometry & helpers

```cpp
// Distances & lengths (floating-point Type only for the first four)
constexpr Type    distance_to(const Vector3&) const noexcept;  // length of (*this - other)
constexpr Type    length()     const noexcept;                 // hypot(x,y,z)
constexpr Type    length_2d()  const noexcept;                 // 2D length from base
constexpr Vector3 normalized() const noexcept;                 // returns *this if length==0
constexpr Type    distance_to_sqr(const Vector3&) const noexcept;
constexpr Type    length_sqr() const noexcept;

// Products
constexpr Type    dot(const Vector3&)   const noexcept;
constexpr Vector3 cross(const Vector3&) const noexcept;  // right-handed

// Sums, tuples & arrays
constexpr Type sum()      const noexcept;  // x + y + z
constexpr Type sum_2d()   const noexcept;  // x + y
constexpr std::tuple<Type,Type,Type> as_tuple() const noexcept;
constexpr std::array<Type, 3>        as_array() const noexcept;

// Utilities
constexpr Vector3& abs() noexcept;        // in-place component-wise absolute
constexpr Vector3  abs() const noexcept;  // non-mutating overload
constexpr bool point_to_same_direction(const Vector3&) const;  // dot(other) > 0
```

---

## Angles & orthogonality

```cpp
enum class Vector3Error { IMPOSSIBLE_BETWEEN_ANGLE };

// Angle in degrees, clamped to [0,180]. Error if any vector has zero length.
// Floating-point Type only.
template<class SelfType = Type>
constexpr std::expected<
  omath::Angle<SelfType, SelfType{0}, SelfType{180}, AngleFlags::Clamped>,
  Vector3Error
> angle_between(const Vector3& other) const noexcept;

// true if the angle is within `epsilon` degrees of 90; false for zero-length vectors.
// Floating-point Type only.
constexpr bool is_perpendicular(const Vector3& other,
                                Type epsilon = static_cast<Type>(0.0001)) const noexcept;
```

---

## Hashing & formatting

* **Hash (any `Type`)**

  ```cpp
  template<class Type>
  struct std::hash<omath::Vector3<Type>> {
    std::size_t operator()(const omath::Vector3<Type>&) const noexcept;
  };
  ```

  Example:

  ```cpp
  std::unordered_map<omath::Vector3<float>, int> counts;
  counts[{1.f, 2.f, 3.f}] = 7;
  ```

* **`std::formatter`** (all character types)

  ```cpp
  template<class Type>
  struct std::formatter<omath::Vector3<Type>>; // prints "[x, y, z]"
  ```

---

## Error handling

* `angle_between()` returns `std::unexpected(Vector3Error::IMPOSSIBLE_BETWEEN_ANGLE)` if either vector length is zero.
* Other operations are total for arithmetic `Type` (no throwing behavior in this class).

---

## Examples

### Cross product & perpendicular check

```cpp
omath::Vector3<float> x{1,0,0}, y{0,1,0};
auto z = x.cross(y);                // (0,0,1)
bool perp = x.is_perpendicular(y);  // true
```

### Safe normalization and angle

```cpp
omath::Vector3<float> u{0,0,0}, v{1,1,0};
auto nu = u.normalized();           // returns {0,0,0}
if (auto ang = u.angle_between(v)) {
  // won't happen: u has zero length → error
} else {
  // handle degenerate case
}
```

### Hadamard vs scalar multiply

```cpp
omath::Vector3<float> a{2,3,4}, b{5,6,7};
auto h = a * b;   // (10, 18, 28) component-wise
auto s = a * 2.f; // (4, 6, 8)    scalar
```

---

## API summary (signatures)

```cpp
// Ctors / cast
constexpr Vector3() noexcept;
constexpr Vector3(const Type& x, const Type& y, const Type& z) noexcept;
template<class CastedType> constexpr explicit operator Vector3<CastedType>() const noexcept;

// Equality & ordering (ordering by length_sqr())
constexpr bool operator==(const Vector3&) const noexcept;
constexpr bool operator!=(const Vector3&) const noexcept;
constexpr bool operator< (const Vector3&) const noexcept;
constexpr bool operator> (const Vector3&) const noexcept;
constexpr bool operator<=(const Vector3&) const noexcept;
constexpr bool operator>=(const Vector3&) const noexcept;

// Mutating arithmetic
constexpr Vector3& operator+=(const Vector3&) noexcept;
constexpr Vector3& operator-=(const Vector3&) noexcept;
constexpr Vector3& operator*=(const Vector3&) noexcept;
constexpr Vector3& operator/=(const Vector3&) noexcept;
constexpr Vector3& operator*=(const Type&)   noexcept;
constexpr Vector3& operator/=(const Type&)   noexcept;
constexpr Vector3& operator+=(const Type&)   noexcept;
constexpr Vector3& operator-=(const Type&)   noexcept;

// Non-mutating arithmetic
constexpr Vector3 operator-() const noexcept;
constexpr Vector3 operator+(const Vector3&) const noexcept;
constexpr Vector3 operator-(const Vector3&) const noexcept;
constexpr Vector3 operator*(const Vector3&) const noexcept;
constexpr Vector3 operator/(const Vector3&) const noexcept;
constexpr Vector3 operator*(const Type&)   const noexcept;
constexpr Vector3 operator/(const Type&)   const noexcept;

// Geometry
constexpr Type    distance_to(const Vector3&) const noexcept;   // floating-point only
constexpr Type    distance_to_sqr(const Vector3&) const noexcept;
constexpr Type    length() const noexcept;                      // floating-point only
constexpr Type    length_2d() const noexcept;                   // floating-point only
constexpr Vector3 normalized() const noexcept;                  // floating-point only
constexpr Type    length_sqr() const noexcept;
constexpr Type    dot(const Vector3&) const noexcept;
constexpr Vector3 cross(const Vector3&) const noexcept;

constexpr Vector3& abs() noexcept;
constexpr Vector3  abs() const noexcept;
constexpr Type sum()    const noexcept;
constexpr Type sum_2d() const noexcept;
constexpr std::tuple<Type,Type,Type> as_tuple() const noexcept;
constexpr std::array<Type,3>         as_array() const noexcept;
constexpr bool point_to_same_direction(const Vector3&) const;

// Angles (floating-point only)
template<class SelfType = Type>
constexpr std::expected<omath::Angle<SelfType, SelfType{0}, SelfType{180}, AngleFlags::Clamped>, omath::Vector3Error>
angle_between(const Vector3&) const noexcept;
constexpr bool is_perpendicular(const Vector3&, Type epsilon = static_cast<Type>(0.0001)) const noexcept;

// std::hash<Vector3<Type>> and std::formatter<Vector3<Type>> specializations are provided below the class
```

---

## Notes

* Inherits all public API of `Vector2<Type>` (including `x`, `y`, many operators, and helpers used internally).
* `normalized()` returns the original vector if its length is zero (no NaNs).
* `cross()` uses the standard right-handed definition.
* `length()`, `length_2d()`, `normalized()`, `distance_to()`, `angle_between()` and `is_perpendicular()` require a floating-point `Type`.

---

## See Also

- [Vector2 Documentation](vector2.md) - 2D vector operations
- [Vector4 Documentation](vector4.md) - 4D vector operations
- [Angle Documentation](../trigonometry/angle.md) - Working with angles
- [Getting Started Guide](../getting_started.md) - Quick start with OMath
- [Tutorials](../tutorials.md) - Practical examples including vector math

---

*Last updated: 1 Nov 2025*
