# `omath::Vector4` — 4D vector (C++20/23)

> Header: `omath/linear_algebra/vector4.hpp`
> Namespace: `omath`
> Template: `template<class Type> requires std::is_arithmetic_v<Type>`
> Inherits: `omath::Vector3<Type>` (brings `x`, `y`, `z` and most scalar ops)

`Vector4<Type>` extends `Vector3<Type>` with a `w` component and 4D operations: component-wise arithmetic, scalar ops, dot/length helpers, clamping, hashing (generic `std::hash` specialization) and `std::formatter` support. Optional ImGui interop is available behind a macro.

---

## Quick start

```cpp
#include <omath/linear_algebra/vector4.hpp>
using omath::Vector4;

using Vec4f = Vector4<float>;

Vec4f a{1, 2, 3, 1};
Vec4f b{4, 5, 6, 2};

// Component-wise & scalar ops
auto c  = a + b;         // (5, 7, 9, 3)
c      *= 0.5f;          // (2.5, 3.5, 4.5, 1.5)
auto h  = a * b;         // Hadamard: (4, 10, 18, 2)

// Dot / length
float d  = a.dot(b);     // 1*4 + 2*5 + 3*6 + 1*2 = 32
float L  = a.length();   // sqrt(x²+y²+z²+w²)

// Clamp (all four components)
Vec4f col{1.4f, -0.2f, 0.7f, 42.f};
col.clamp(0.f, 1.f);     // -> (1, 0, 0.7, 1)
```

---

## Data members

```cpp
// Inherited from Vector3<Type>:
Type x{0};
Type y{0};
Type z{0};

// Added in Vector4:
Type w{0};
```

---

## Constructors

```cpp
constexpr Vector4() noexcept;                                      // (0,0,0,0)
constexpr Vector4(const Type& x, const Type& y,
                  const Type& z, const Type& w);                    // value-init

// explicit cast to another arithmetic component type
template<class CastedType> requires std::is_arithmetic_v<CastedType>
constexpr explicit operator Vector4<CastedType>() const noexcept;
```

`using ContainedType = Type;` is also declared.

---

## Equality & ordering

```cpp
constexpr bool operator==(const Vector4&) const noexcept;           // component-wise
constexpr bool operator!=(const Vector4&) const noexcept;

constexpr bool operator< (const Vector4&) const noexcept; // compare by length_sqr()
constexpr bool operator> (const Vector4&) const noexcept;
constexpr bool operator<=(const Vector4&) const noexcept;
constexpr bool operator>=(const Vector4&) const noexcept;
```

> **Note:** Ordering uses **magnitude** (squared Euclidean norm), not lexicographic order.

---

## Arithmetic (mutating)

With another vector (component-wise):

```cpp
constexpr Vector4& operator+=(const Vector4&) noexcept;
constexpr Vector4& operator-=(const Vector4&) noexcept;
constexpr Vector4& operator*=(const Vector4&) noexcept;   // Hadamard
constexpr Vector4& operator/=(const Vector4&) noexcept;
```

With a scalar:

```cpp
constexpr Vector4& operator*=(const Type& v) noexcept;
constexpr Vector4& operator/=(const Type& v) noexcept;
constexpr Vector4& operator+=(const Type& v) noexcept;    // adds v to x, y, z and w
constexpr Vector4& operator-=(const Type& v) noexcept;
```

---

## Arithmetic (non-mutating)

```cpp
constexpr Vector4 operator-()                      const noexcept;
constexpr Vector4 operator+(const Vector4&)        const noexcept;
constexpr Vector4 operator-(const Vector4&)        const noexcept;
constexpr Vector4 operator*(const Vector4&)        const noexcept;   // Hadamard
constexpr Vector4 operator/(const Vector4&)        const noexcept;   // Hadamard
constexpr Vector4 operator*(const Type& scalar)    const noexcept;
constexpr Vector4 operator/(const Type& scalar)    const noexcept;
```

---

## Geometry & helpers

```cpp
constexpr Type    length_sqr()  const noexcept;     // x² + y² + z² + w²
constexpr Type    length()      const noexcept;     // internal::hypot(x,y,z,w); floating-point only
constexpr Type    distance_to_sqr(const Vector4&) const noexcept;
constexpr Type    distance_to(const Vector4&)     const noexcept;  // floating-point only
constexpr Vector4 normalized()  const noexcept;     // floating-point only; returns *this if length==0
constexpr Type    dot(const Vector4& rhs) const noexcept;

constexpr Vector4& abs() noexcept;                  // in-place component-wise absolute
constexpr Vector4  abs() const noexcept;            // non-mutating overload
constexpr Vector4& clamp(const Type& min, const Type& max) noexcept;  // clamps x, y, z and w
constexpr Type    sum()         const noexcept;     // x + y + z + w
constexpr std::array<Type, 4> as_array() const noexcept;
constexpr bool point_to_same_direction(const Vector4&) const noexcept;  // dot(other) > 0

// Floating-point only
template<class SelfType = Type>
constexpr std::expected<omath::Angle<SelfType, SelfType{0}, SelfType{180}, AngleFlags::Clamped>, Vector3Error>
angle_between(const Vector4&) const noexcept;
constexpr bool is_perpendicular(const Vector4&, Type epsilon = static_cast<Type>(0.0001)) const noexcept;
```

---

## ImGui integration (optional)

Guarded by `OMATH_IMGUI_INTEGRATION`:

```cpp
#ifdef OMATH_IMGUI_INTEGRATION
constexpr ImVec4 to_im_vec4() const noexcept;
static Vector4   from_im_vec4(const ImVec4& other) noexcept;
#endif
```

---

## Hashing & formatting

* **Hash specialization** (any `Type`):

  ```cpp
  template<class Type>
  struct std::hash<omath::Vector4<Type>> {
    std::size_t operator()(const omath::Vector4<Type>&) const noexcept;
  };
  ```

  Example:

  ```cpp
  std::unordered_map<omath::Vector4<float>, int> counts;
  counts[{1.f, 2.f, 3.f, 1.f}] = 7;
  ```

* **`std::formatter`** (for any `Type`, all character kinds):

  ```cpp
  template<class Type>
  struct std::formatter<omath::Vector4<Type>>;   // -> "[x, y, z, w]"
  ```

---

## Notes

* `clamp(min, max)` clamps all four components, including `w`.
* `length()`, `distance_to()`, `normalized()`, `angle_between()` and `is_perpendicular()` require a floating-point `Type`.
* `angle_between` returns `std::unexpected(Vector3Error::IMPOSSIBLE_BETWEEN_ANGLE)` if either vector has zero length.
* **ImGui interop**: the header references `ImVec4` but does not include `<imgui.h>` itself. Ensure it is included **before** this header whenever `OMATH_IMGUI_INTEGRATION` is defined. (`Vector3`/`Vector2` headers include it only through `vector2.hpp` when the macro is set.)
* `Vector4` inherits all other `Vector3`/`Vector2` members (e.g. `cross`, `length_2d`); those operate on the base components only.

---

## API summary (signatures)

```cpp
// Ctors / cast
constexpr Vector4() noexcept;
constexpr Vector4(const Type& x, const Type& y, const Type& z, const Type& w);
template<class CastedType> constexpr explicit operator Vector4<CastedType>() const noexcept;

// Equality & ordering (ordering by length_sqr())
constexpr bool operator==(const Vector4&) const noexcept;
constexpr bool operator!=(const Vector4&) const noexcept;
constexpr bool operator< (const Vector4&) const noexcept;
constexpr bool operator> (const Vector4&) const noexcept;
constexpr bool operator<=(const Vector4&) const noexcept;
constexpr bool operator>=(const Vector4&) const noexcept;

// Mutating arithmetic
constexpr Vector4& operator+=(const Vector4&) noexcept;
constexpr Vector4& operator-=(const Vector4&) noexcept;
constexpr Vector4& operator*=(const Vector4&) noexcept;
constexpr Vector4& operator/=(const Vector4&) noexcept;
constexpr Vector4& operator+=(const Type&)    noexcept;
constexpr Vector4& operator-=(const Type&)    noexcept;
constexpr Vector4& operator*=(const Type&)    noexcept;
constexpr Vector4& operator/=(const Type&)    noexcept;

// Non-mutating arithmetic
constexpr Vector4 operator-() const noexcept;
constexpr Vector4 operator+(const Vector4&) const noexcept;
constexpr Vector4 operator-(const Vector4&) const noexcept;
constexpr Vector4 operator*(const Vector4&) const noexcept;
constexpr Vector4 operator/(const Vector4&) const noexcept;
constexpr Vector4 operator*(const Type&)    const noexcept;
constexpr Vector4 operator/(const Type&)    const noexcept;

// Geometry & helpers
constexpr Type    length_sqr() const noexcept;
constexpr Type    length() const noexcept;                     // floating-point only
constexpr Type    distance_to_sqr(const Vector4&) const noexcept;
constexpr Type    distance_to(const Vector4&) const noexcept;  // floating-point only
constexpr Vector4 normalized() const noexcept;                 // floating-point only
constexpr Type    dot(const Vector4&) const noexcept;
constexpr Vector4& abs() noexcept;
constexpr Vector4  abs() const noexcept;
constexpr Vector4& clamp(const Type& min, const Type& max) noexcept;
constexpr Type    sum() const noexcept;
constexpr std::array<Type, 4> as_array() const noexcept;
constexpr bool    point_to_same_direction(const Vector4&) const noexcept;

// Angles (floating-point only)
template<class SelfType = Type>
constexpr std::expected<omath::Angle<SelfType, SelfType{0}, SelfType{180}, AngleFlags::Clamped>, Vector3Error>
angle_between(const Vector4&) const noexcept;
constexpr bool is_perpendicular(const Vector4&, Type epsilon = static_cast<Type>(0.0001)) const noexcept;

// ImGui (optional)
#ifdef OMATH_IMGUI_INTEGRATION
constexpr ImVec4 to_im_vec4() const noexcept;
static Vector4   from_im_vec4(const ImVec4&) noexcept;
#endif

// std::hash<Vector4<Type>> and std::formatter<Vector4<Type>> specializations provided below the class
```

---

*Last updated: 31 Oct 2025*
