# `omath::collision::GjkAlgorithm` — Gilbert-Johnson-Keerthi collision detection

> Header: `omath/collision/gjk_algorithm.hpp`
> Namespace: `omath::collision`
> Depends on: `Simplex<VectorType>`, collider types derived from `ColliderInterface` (`find_abs_furthest_vertex_position`, `get_origin`)
> Algorithm: **GJK** (Gilbert-Johnson-Keerthi) for convex shape collision detection

---

## Overview

The **GJK algorithm** determines whether two convex shapes intersect by iteratively constructing a simplex in Minkowski difference space. The algorithm is widely used in physics engines and collision detection systems due to its efficiency and robustness.

`GjkAlgorithm` is a template class that works with any collider type exposing a `VectorType` alias and the support function interface (see [`ColliderInterface`](collider_interface.md)):

* `find_abs_furthest_vertex_position(direction)` — returns the farthest point in the collider (in world space) along the given direction.
* `get_origin()` — optional for GJK itself; when present it is used to pick the initial search direction.

The algorithm returns a `GjkHitInfo` containing:
* `hit` — boolean indicating whether the shapes intersect
* `simplex` — a 4-point simplex containing the origin (valid only when `hit == true`)

Settings can be tuned with `GjkSettings`.

---

## `GjkHitInfo`

```cpp
template<class VertexType>
struct GjkHitInfo final {
    bool hit{false};                    // true if collision detected
    Simplex<VertexType> simplex;        // 4-point simplex (valid only if hit == true)
};
```

The `simplex` field is only meaningful when `hit == true` and contains 4 points. This simplex can be passed to the EPA algorithm for penetration depth calculation.

---

## `GjkSettings`

```cpp
struct GjkSettings final {
    float       epsilon        = 1e-6f;  // progress / zero-length tolerance
    std::size_t max_iterations = 64;     // iteration cap
};
```

---

## `GjkAlgorithm`

```cpp
template<class ColliderInterfaceType>
class GjkAlgorithm final {
    using VectorType = ColliderInterfaceType::VectorType;

public:
    // Support point of the Minkowski difference A - B along `direction`
    [[nodiscard]]
    static VectorType find_support_vertex(
        const ColliderInterfaceType& collider_a,
        const ColliderInterfaceType& collider_b,
        const VectorType& direction
    );

    // Check if two convex shapes intersect
    [[nodiscard]]
    static bool is_collide(
        const ColliderInterfaceType& collider_a,
        const ColliderInterfaceType& collider_b
    );

    // Same, but also returns the final simplex (input for EPA)
    [[nodiscard]]
    static GjkHitInfo<VectorType> is_collide_with_simplex_info(
        const ColliderInterfaceType& collider_a,
        const ColliderInterfaceType& collider_b,
        const GjkSettings& settings = {}
    );
};
```

---

## Collider Requirements

Any type used as `ColliderInterfaceType` must provide:

```cpp
// Type alias for the vector type (typically Vector3<float>)
using VectorType = /* ... */;

// Find the farthest point in world space along the given direction
[[nodiscard]]
VectorType find_abs_furthest_vertex_position(const VectorType& direction) const;

// Optional: used to seed the initial search direction (origin_b - origin_a)
[[nodiscard]]
const VectorType& get_origin() const;
```

The `omath::collision::ColliderInterface<VecType>` base class declares these (plus `set_origin`).

Common collider types:
* `MeshCollider<MeshType>` — for arbitrary triangle meshes (`omath::source_engine::MeshCollider` and `omath::serious_engine::MeshCollider` are ready-made aliases)
* Custom colliders for spheres, boxes, capsules, etc.

---

## Algorithm Details

### Minkowski Difference

GJK operates in the **Minkowski difference** space `A - B`, where a point in this space represents the difference between points in shapes A and B. The shapes intersect if and only if the origin lies within this Minkowski difference.

### Support Function

The support function finds the point in the Minkowski difference farthest along a given direction:

```cpp
support(A, B, dir) = A.find_abs_furthest_vertex_position(dir) - B.find_abs_furthest_vertex_position(-dir)
```

This is computed by `find_support_vertex`.

### Simplex Iteration

The algorithm builds a simplex incrementally:
1. Start with an initial direction (`get_origin()` of B minus `get_origin()` of A when available; `{1, 0, 0}` if the origins coincide or the collider has no `get_origin()`)
2. Add support vertices in directions that move the simplex toward the origin
3. Simplify the simplex to keep only points closest to the origin
4. Repeat until either:
   * Origin is contained (collision detected, returns 4-point simplex)
   * No progress can be made (no collision)

At most `GjkSettings::max_iterations` (default 64) iterations are performed to prevent infinite loops in edge cases. A new support point that does not pass the origin along the search direction (`support.dot(direction) <= epsilon`) ends the search with no hit.

---

## Usage Examples

### Basic Collision Check

```cpp
using namespace omath::collision;
using namespace omath::source_engine;

// Create mesh colliders
Mesh mesh_a = /* ... */;
Mesh mesh_b = /* ... */;

MeshCollider collider_a(mesh_a);   // omath::source_engine::MeshCollider
MeshCollider collider_b(mesh_b);

using Gjk = GjkAlgorithm<MeshCollider>;

// Boolean check
if (Gjk::is_collide(collider_a, collider_b)) {
    std::cout << "Collision detected!\n";
}

// Or keep the simplex for EPA
const auto result = Gjk::is_collide_with_simplex_info(collider_a, collider_b);
if (result.hit) {
    // Can pass result.simplex to EPA for penetration depth
}
```

### Combined with EPA

```cpp
auto gjk_result = GjkAlgorithm<Collider>::is_collide_with_simplex_info(a, b);

if (gjk_result.hit) {
    // Get penetration depth and normal using EPA
    auto epa_result = Epa<Collider>::solve(
        a, b, gjk_result.simplex
    );

    if (epa_result) {   // std::optional<Epa<Collider>::Result>
        std::cout << "Penetration depth: " << epa_result->depth << "\n";
        std::cout << "Separation normal: " << epa_result->normal << "\n";
    }
}
```

---

## Performance Characteristics

* **Time complexity**: O(k) where k is the number of iterations (typically < 20 for most cases)
* **Space complexity**: O(1) — only stores a 4-point simplex
* **Best case**: 4-8 iterations for well-separated objects
* **Worst case**: `GjkSettings::max_iterations` iterations (default 64)
* **Cache efficient**: operates on small fixed-size data structures

### Optimization Tips

1. **Initial direction**: The implementation already seeds from the vector between collider origins when `get_origin()` exists
2. **Early exit**: GJK quickly rejects non-intersecting shapes
3. **Warm starting**: Reuse previous simplex for continuous collision detection
4. **Broad phase**: Use spatial partitioning before GJK (AABB trees, grids)

---

## Limitations & Edge Cases

* **Convex shapes only**: GJK only works with convex colliders. For concave shapes, decompose into convex parts or use a mesh collider wrapper.
* **Degenerate simplices**: The algorithm handles degenerate cases, but numerical precision can cause issues with very thin or flat shapes.
* **Iteration limit**: The `GjkSettings::max_iterations` cap (default 64) prevents infinite loops but may miss collisions in extreme cases; hitting the cap reports no hit.
* **Zero-length directions**: The simplex update logic guards against zero-length vectors, returning safe fallbacks.

---

## Vertex Type Requirements

The collider's `VectorType` must satisfy the `GjkVector` concept (defined in `simplex.hpp`):

```cpp
template<class V>
concept GjkVector = requires(const V& a, const V& b) {
    { -a } -> std::same_as<V>;
    { a - b } -> std::same_as<V>;
    { a.cross(b) } -> std::same_as<V>;
    { a.point_to_same_direction(b) } -> std::same_as<bool>;
};
```

`omath::Vector3<float>` satisfies this concept.

---

## See Also

- [EPA Algorithm Documentation](epa_algorithm.md) - Penetration depth calculation
- [Simplex Documentation](simplex.md) - Simplex data structure
- [ColliderInterface Documentation](collider_interface.md) - Collider base class
- [MeshCollider Documentation](mesh_collider.md) - Mesh-based collider
- [Mesh Documentation](../3d_primitives/mesh.md) - Mesh primitive
- [LineTracer Documentation](line_tracer.md) - Ray-triangle intersection
- [Tutorials - Collision Detection](../tutorials.md#tutorial-4-collision-detection) - Complete collision tutorial

---

*Last updated: 13 Nov 2025*
