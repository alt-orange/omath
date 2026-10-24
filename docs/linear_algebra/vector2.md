# `omath::Vector2` — 2D vector (C++20/23)

> Header: `omath/linear_algebra/vector2.hpp`
> Namespace: `omath`
> Template: `template<class Type> requires std::is_arithmetic_v<Type>`

`Vector2<Type>` is a lightweight, POD-like 2D math type with arithmetic, geometry helpers, comparisons, hashing (generic `std::hash` specialization), optional ImGui interop, and `std::formatter` support.

---

## Quick start

```cpp
#include <omath/linear_algebra/vector2.hpp>
using omath::Vector2;

using Vec2f = Vector2<float>;

Vec2f a{3.f, 4.f};
Vec2f b{1.f, 2.f};

auto d      = a.distance_to(b);     //  ≈ 3.1623
auto dot    = a.dot(b);             //  11
auto len    = a.length();           //  5
auto unit_a = a.normalized();       //  (0.6, 0.8)

// Component-wise mutate
Vec2f c{2, 3};
c *= b;                              // c -> (2*1, 3*2) = (2, 6)

// Scalar ops (non-mutating + mutating)
auto scaled = a * 0.5f;             // (1.5, 2)
a *= 2.f;                           // (6, 8)

// Ordering by squared length (same order as length())
bool shorter = (b < a);

// Formatted printing
std::string s = std::format("a = {}", a);  // "a = [6, 8]"
```

---

## Members

```cpp
Type x{0};
Type y{0};
```

---

## Constructors

```cpp
constexpr Vector2() = default;                   // (0,0)
constexpr Vector2(const Type& x, const Type& y) noexcept;

// explicit cast to another arithmetic component type
template<class CastedType> requires std::is_arithmetic_v<CastedType>
constexpr explicit operator Vector2<CastedType>() const noexcept;
```

`using ContainedType = Type;` is also declared.

---

## Equality & ordering

```cpp
constexpr bool operator==(const Vector2&) const noexcept; // component-wise equality
constexpr bool operator!=(const Vector2&) const noexcept;

constexpr bool operator< (const Vector2&) const noexcept; // compares by length_sqr()
constexpr bool operator> (const Vector2&) const noexcept;
constexpr bool operator<=(const Vector2&) const noexcept;
constexpr bool operator>=(const Vector2&) const noexcept;
```

> **Note:** `<`, `>`, `<=`, `>=` order vectors by **magnitude** (squared length; not lexicographically).

---

## Arithmetic

### With another vector (component-wise, **mutating**)

```cpp
constexpr Vector2& operator+=(const Vector2&) noexcept;
constexpr Vector2& operator-=(const Vector2&) noexcept;
constexpr Vector2& operator*=(const Vector2&) noexcept;  // Hadamard product (x*=x, y*=y)
constexpr Vector2& operator/=(const Vector2&) noexcept;
```

### With a scalar

```cpp
constexpr Vector2& operator*=(const Type& v) noexcept;
constexpr Vector2& operator/=(const Type& v) noexcept;
constexpr Vector2& operator+=(const Type& v) noexcept;
constexpr Vector2& operator-=(const Type& v) noexcept;

constexpr Vector2 operator*(const Type& v) const noexcept;
constexpr Vector2 operator/(const Type& v) const noexcept;
```

### Binary with another vector (non-mutating)

```cpp
constexpr Vector2 operator+(const Vector2&) const noexcept;
constexpr Vector2 operator-(const Vector2&) const noexcept;
constexpr Vector2 operator*(const Vector2&) const noexcept;  // component-wise
constexpr Vector2 operator/(const Vector2&) const noexcept;  // component-wise
```

### Unary

```cpp
constexpr Vector2 operator-() const noexcept;   // negation
```

---

## Geometry & helpers

```cpp
constexpr Type distance_to    (const Vector2&) const noexcept requires std::is_floating_point_v<Type>;
constexpr Type distance_to_sqr(const Vector2&) const noexcept;

constexpr Type dot(const Vector2&) const noexcept;

constexpr Type    length()     const noexcept requires std::is_floating_point_v<Type>;  // internal::hypot
constexpr Vector2 normalized() const noexcept requires std::is_floating_point_v<Type>;  // returns *this if length==0

constexpr Type     length_sqr() const noexcept;  // x*x + y*y
constexpr Vector2& abs() noexcept;               // in-place component-wise absolute
constexpr Vector2  abs() const noexcept;         // non-mutating overload

constexpr Type sum() const noexcept;             // x + y
constexpr std::tuple<Type, Type> as_tuple() const noexcept;
constexpr std::array<Type, 2>    as_array() const noexcept;
```

---

## ImGui integration (optional)

Define `OMATH_IMGUI_INTEGRATION` **before** including the header.

```cpp
#ifdef OMATH_IMGUI_INTEGRATION
constexpr ImVec2 to_im_vec2() const noexcept;         // {float(x), float(y)}
static Vector2   from_im_vec2(const ImVec2&) noexcept;
#endif
```

---

## Hashing & formatting

* **Hash (any `Type`)**

  ```cpp
  template<class Type>
  struct std::hash<omath::Vector2<Type>> {
      std::size_t operator()(const omath::Vector2<Type>&) const noexcept;
  };
  ```

  Example:

  ```cpp
  std::unordered_set<omath::Vector2<float>> set;
  set.insert({1.f, 2.f});
  ```

* **`std::formatter`** (for any `Type`)

  ```cpp
  // prints "[x, y]" for char / wchar_t / char8_t
  template<class Type>
  struct std::formatter<omath::Vector2<Type>>;
  ```

---

## Notes & invariants

* `Type` must be arithmetic (e.g., `float`, `double`, `int`, …).
* `length()`, `normalized()` and `distance_to()` are only available for floating-point `Type`.
* `normalized()` returns the input unchanged if `length() == 0`.
* `abs()` uses a constexpr-friendly implementation (not `std::abs`) to allow compile-time evaluation.

---

## Examples

### Component-wise operations and scalar scaling

```cpp
omath::Vector2<float> u{2, 3}, v{4, 5};

u += v;                // (6, 8)
u -= v;                // (2, 3)
u *= v;                // (8, 15)  Hadamard product (mutates u)
auto w = v * 2.0f;     // (8, 10)  non-mutating scalar multiply
```

### Geometry helpers

```cpp
omath::Vector2<double> p{0.0, 0.0}, q{3.0, 4.0};

auto dsq = p.distance_to_sqr(q);   // 25
auto d   = p.distance_to(q);       // 5
auto dot = p.dot(q);               // 0
auto uq  = q.normalized();         // (0.6, 0.8)
```

### Using as a key in unordered containers

```cpp
std::unordered_map<omath::Vector2<float>, int> counts;
counts[{1.f, 2.f}] = 42;
```

### ImGui interop

```cpp
#define OMATH_IMGUI_INTEGRATION
#include <omath/linear_algebra/vector2.hpp>

omath::Vector2<float> v{10, 20};
ImVec2 iv = v.to_im_vec2();
v = omath::Vector2<float>::from_im_vec2(iv);
```

---

## API summary (signatures)

```cpp
// Constructors / cast
constexpr Vector2() = default;
constexpr Vector2(const Type& x, const Type& y) noexcept;
template<class CastedType> constexpr explicit operator Vector2<CastedType>() const noexcept;

// Equality & ordering (ordering by length_sqr())
constexpr bool operator==(const Vector2&) const noexcept;
constexpr bool operator!=(const Vector2&) const noexcept;
constexpr bool operator< (const Vector2&) const noexcept;
constexpr bool operator> (const Vector2&) const noexcept;
constexpr bool operator<=(const Vector2&) const noexcept;
constexpr bool operator>=(const Vector2&) const noexcept;

// Compound (vector/vector and scalar)
constexpr Vector2& operator+=(const Vector2&) noexcept;
constexpr Vector2& operator-=(const Vector2&) noexcept;
constexpr Vector2& operator*=(const Vector2&) noexcept;
constexpr Vector2& operator/=(const Vector2&) noexcept;
constexpr Vector2& operator*=(const Type&) noexcept;
constexpr Vector2& operator/=(const Type&) noexcept;
constexpr Vector2& operator+=(const Type&) noexcept;
constexpr Vector2& operator-=(const Type&) noexcept;

// Non-mutating arithmetic
constexpr Vector2 operator+(const Vector2&) const noexcept;
constexpr Vector2 operator-(const Vector2&) const noexcept;
constexpr Vector2 operator*(const Vector2&) const noexcept;
constexpr Vector2 operator/(const Vector2&) const noexcept;
constexpr Vector2 operator*(const Type&) const noexcept;
constexpr Vector2 operator/(const Type&) const noexcept;
constexpr Vector2 operator-() const noexcept;

// Geometry
constexpr Type    distance_to(const Vector2&) const noexcept;   // floating-point only
constexpr Type    distance_to_sqr(const Vector2&) const noexcept;
constexpr Type    dot(const Vector2&) const noexcept;
constexpr Type    length() const noexcept;                      // floating-point only
constexpr Vector2 normalized() const noexcept;                  // floating-point only
constexpr Type    length_sqr() const noexcept;
constexpr Vector2& abs() noexcept;
constexpr Vector2  abs() const noexcept;
constexpr Type    sum() const noexcept;
constexpr std::tuple<Type,Type> as_tuple() const noexcept;
constexpr std::array<Type,2>    as_array() const noexcept;

// ImGui (optional)
#ifdef OMATH_IMGUI_INTEGRATION
constexpr ImVec2 to_im_vec2() const noexcept;
static Vector2   from_im_vec2(const ImVec2&) noexcept;
#endif

// std::hash<Vector2<Type>> and std::formatter<Vector2<Type>> are specialized in the header
```

---

## See Also

- [Vector3 Documentation](vector3.md) - 3D vector operations
- [Vector4 Documentation](vector4.md) - 4D vector operations
- [Getting Started Guide](../getting_started.md) - Quick start with OMath
- [Tutorials](../tutorials.md) - Step-by-step examples

---

*Last updated: 1 Nov 2025*
