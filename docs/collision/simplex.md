# `omath::collision::Simplex` — Fixed-capacity simplex for GJK/EPA

> Header: `omath/collision/simplex.hpp`
> Namespace: `omath::collision`
> Depends on: `Vector3<float>` (or any type satisfying `GjkVector` concept)
> Purpose: store and manipulate simplices in GJK and EPA algorithms

---

## Overview

`Simplex` is a lightweight container for up to 4 points, used internally by the GJK and EPA collision detection algorithms. A simplex in this context is a geometric shape defined by 1 to 4 vertices:

* **1 point** — a single vertex
* **2 points** — a line segment
* **3 points** — a triangle
* **4 points** — a tetrahedron

The GJK algorithm builds simplices incrementally to detect collisions, and EPA extends a 4-point simplex to compute penetration depth.

---

## Template & Concepts

```cpp
template<GjkVector VectorType = Vector3<float>>
class Simplex final;
```

### `GjkVector` Concept

The vertex type must satisfy:

```cpp
template<class V>
concept GjkVector = requires(const V& a, const V& b) {
    { -a } -> std::same_as<V>;
    { a - b } -> std::same_as<V>;
    { a.cross(b) } -> std::same_as<V>;
    { a.point_to_same_direction(b) } -> std::same_as<bool>;
};
```

`omath::Vector3<float>` satisfies this concept and is the default (`template<GjkVector VectorType = Vector3<float>>`).

---

## Constructors & Assignment

```cpp
constexpr Simplex() = default;

constexpr Simplex& operator=(std::initializer_list<VectorType> list) noexcept;
```

`static constexpr std::size_t capacity = 4;`

### Initialization

```cpp
// Empty simplex
Simplex<Vector3<float>> s;

// Initialize with points
Simplex<Vector3<float>> s2;
s2 = {v1, v2, v3};  // 3-point simplex (triangle)
```

**Constraint**: Maximum 4 points. Passing more triggers an assertion in debug builds.

---

## Core Methods

### Adding Points

```cpp
constexpr void push_front(const VectorType& p) noexcept;
```

Inserts a point at the **front** (index 0), shifting existing points back. If the simplex is already at capacity (4 points), the last point is discarded.

**Usage pattern in GJK**:
```cpp
simplex.push_front(new_support_point);
// Now simplex[0] is the newest point
```

### Size & Capacity

```cpp
[[nodiscard]] constexpr std::size_t size() const noexcept;
[[nodiscard]] constexpr bool        empty() const noexcept;
static constexpr std::size_t capacity = 4;
constexpr void clear() noexcept;   // size becomes 0
```

* `size()` — current number of points (0-4)
* `capacity` — maximum points (always 4)

### Element Access

```cpp
[[nodiscard]] constexpr VectorType& operator[](std::size_t index) noexcept;
[[nodiscard]] constexpr const VectorType& operator[](std::size_t index) const noexcept;
```

Access points by index. **No bounds checking** — index must be `< size()`.

```cpp
[[nodiscard]] constexpr const VectorType& front() const noexcept;  // newest point (index 0)
[[nodiscard]] constexpr const VectorType& back()  const noexcept;  // oldest point (index size()-1)
[[nodiscard]] constexpr const VectorType* data()  const noexcept;
```

```cpp
if (simplex.size() >= 2) {
    auto edge = simplex[1] - simplex[0];
}
```

### Iterators

```cpp
[[nodiscard]] constexpr auto begin() const noexcept;
[[nodiscard]] constexpr auto end() const noexcept;
```

Only const iteration is provided; `end()` is `begin() + size()`. Range-based loops cover just the live points:

```cpp
for (const auto& vertex : simplex) {
    std::cout << vertex << "\n";
}
```

---

## GJK-Specific Methods

### `handle`

```cpp
[[nodiscard]] constexpr bool handle(VectorType& direction) noexcept;
```

One GJK step. It dispatches on the current size (point, line, triangle, tetrahedron), reduces the simplex to the feature closest to the origin, and writes the **next search direction** into `direction`.

**Return value**:
* `true` — the simplex is a tetrahedron that contains the origin (collision detected)
* `false` — the origin is not contained yet; `direction` has been updated and the simplex may have shrunk (its `size()` can decrease)

With size 0 it returns `false` and leaves `direction` untouched. The remaining logic (`handle_point`, `handle_line`, `handle_triangle`, `handle_tetrahedron`) is private.

---

## Usage Examples

### GJK Iteration (Simplified)

This mirrors what `GjkAlgorithm::is_collide_with_simplex_info` does:

```cpp
Simplex<Vector3<float>> simplex;

auto support = find_support_vertex(collider_a, collider_b, initial_direction);
simplex.push_front(support);
auto direction = -support;

for (int i = 0; i < 64; ++i) {
    // Get support point in current direction
    support = find_support_vertex(collider_a, collider_b, direction);

    // Check if we made progress
    if (support.dot(direction) <= epsilon)
        break;  // No collision possible

    simplex.push_front(support);

    // Reduce the simplex and compute the next direction; true when the origin is enclosed
    if (simplex.handle(direction))
        return GjkHitInfo{true, simplex};   // simplex.size() == 4
}

// No collision
return GjkHitInfo{false, simplex};
```

### Manual Simplex Construction

```cpp
using Vec3 = Vector3<float>;

Simplex<Vec3> simplex;
simplex = {
    Vec3{0.0f, 0.0f, 0.0f},
    Vec3{1.0f, 0.0f, 0.0f},
    Vec3{0.0f, 1.0f, 0.0f},
    Vec3{0.0f, 0.0f, 1.0f}
};

assert(simplex.size() == 4);

// Check if origin is inside this tetrahedron
Vector3<float> direction;
bool has_collision = simplex.handle(direction);
```

### Iterating Over Points

```cpp
void print_simplex(const Simplex<Vector3<float>>& s) {
    std::cout << "Simplex with " << s.size() << " points:\n";
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto& p = s[i];
        std::cout << "  [" << i << "] = (" 
                  << p.x << ", " << p.y << ", " << p.z << ")\n";
    }
}
```

---

## Implementation Details

### Simplex Simplification

`handle()` implements a different test per simplex size:

#### Point (1 point)

Sets the direction toward the origin (`-A`) and returns `false`.

#### Line Segment (2 points)

If the origin lies on the segment's side (`AB` points toward the origin), keeps both points and picks a direction perpendicular to the line toward the origin; if the cross product is degenerate (origin collinear with the segment) it falls back to an arbitrary perpendicular. Otherwise it reduces to the newest point `A`.

#### Triangle (3 points)

Tests the origin against the triangle's edge regions using cross products. Simplifies to:
* An edge (`AC` or `AB`) if the origin lies in that edge's region
* The full triangle if the origin is above or below it (direction along or against the face normal, flipping the winding when needed)

#### Tetrahedron (4 points)

Tests the origin against the three faces that contain the newest point `A`:
* If the origin is outside one of them, reduces to that triangle and runs the triangle case
* If it is inside all three, returns `true` (collision)

### Direction Calculation

The new search direction is computed by `handle()` as an output parameter (`direction`), using cross products and `point_to_same_direction`; no square roots are needed.

---

## Performance Characteristics

* **Storage**: Fixed 4 × `sizeof(VectorType)` + size counter
* **Push front**: O(n) where n is current size (max 4, so effectively O(1))
* **`handle()`**: O(1) for each case (point, line, triangle, tetrahedron); the direction update uses only cross products and subtractions
* **No heap allocations**: All storage is inline

**constexpr**: All methods are `constexpr`, enabling compile-time usage where feasible.

---

## Edge Cases & Constraints

### Degenerate Simplices

* **Zero-length edges**: Can occur if support points coincide. No division is performed; the line case falls back to an arbitrary perpendicular direction when the cross product is (near) zero.
* **Flat tetrahedron**: If the 4th point is coplanar with the first 3, the origin containment test may have reduced precision.

### Assertions

* **Capacity**: `operator=` asserts `list.size() <= 4` in debug builds
* **Index bounds**: No bounds checking in release builds — ensure `index < size()`

### Thread Safety

* **Read-only**: Safe to read from multiple threads
* **Modification**: Not thread-safe; synchronize writes externally

---

## Relationship to GJK & EPA

### In GJK

* Starts empty or with an initial point
* Grows via `push_front` as support points are added
* Shrinks via `handle()` as it's simplified
* Once it reaches 4 points and contains origin, GJK succeeds

### In EPA

* Takes a 4-point simplex from GJK as input
* Uses the tetrahedron as the initial polytope
* Does not directly use the `Simplex` class for expansion (EPA maintains a more complex polytope structure)

---

## See Also

- [GJK Algorithm Documentation](gjk_algorithm.md) - Uses `Simplex` for collision detection
- [EPA Algorithm Documentation](epa_algorithm.md) - Takes 4-point `Simplex` as input
- [MeshCollider Documentation](mesh_collider.md) - Provides support function for GJK/EPA
- [Vector3 Documentation](../linear_algebra/vector3.md) - Default vertex type
- [Tutorials - Collision Detection](../tutorials.md#tutorial-4-collision-detection) - Collision tutorial

---

*Last updated: 13 Nov 2025*
