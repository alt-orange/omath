# `omath::collision::Epa` — Expanding Polytope Algorithm for penetration depth

> Header: `omath/collision/epa_algorithm.hpp`
> Namespace: `omath::collision`
> Depends on: `Simplex<VectorType>`, collider types derived from `ColliderInterface` (`find_abs_furthest_vertex_position`)
> Algorithm: **EPA** (Expanding Polytope Algorithm) for penetration depth and contact normal

---

## Overview

The **EPA (Expanding Polytope Algorithm)** calculates the **penetration depth** and **separation normal** between two intersecting convex shapes. It is typically used as a follow-up to the GJK algorithm after a collision has been detected.

EPA takes a 4-point simplex containing the origin (from GJK) and iteratively expands it to find the point on the Minkowski difference closest to the origin. This point gives both:
* **Depth**: minimum translation distance to separate the shapes
* **Normal**: direction of separation (the header documents it as pointing from A to B)

`Epa` is a template class working with any collider type that implements the support function interface.

---

## `Epa::Result`

```cpp
struct Result final {
    VectorType   normal{};              // unit face normal, from A to B
    VectorType   penetration_vector;    // normal * depth
    FloatingType depth{0.0};            // penetration depth
    int          iterations{0};         // number of iterations performed
    int          num_vertices{0};       // final polytope vertex count
    int          num_faces{0};          // final polytope face count
};
```

`FloatingType` is `VectorType::ContainedType` (e.g. `float`). There is no `success` flag: `solve` returns `std::optional<Result>` and an empty optional signals failure.

### Fields

* `normal` — unit vector of the closest polytope face (header comment: from A to B)
* `depth` — distance from the origin to that face, i.e. the minimum translation along `normal` that separates the shapes
* `penetration_vector` — `normal * depth`
* `iterations` — iteration count (useful for performance tuning)
* `num_vertices`, `num_faces` — final polytope size (for diagnostics)

---

## `Epa::Params`

```cpp
struct Params final {
    int          max_iterations{64};     // maximum iterations before giving up
    FloatingType tolerance{1e-4f};       // absolute tolerance on distance growth
};
```

### Fields

* `max_iterations` — safety limit to prevent infinite loops (default 64)
* `tolerance` — convergence threshold: stop when distance grows less than this (default 1e-4)

---

## `Epa` Template Class

```cpp
template<class ColliderInterfaceType>
class Epa final {
public:
    using VectorType = ColliderInterfaceType::VectorType;
    static_assert(EpaVector<VectorType, typename VectorType::ContainedType>,
                  "VertexType must satisfy EpaVector concept");

    // Solve for penetration depth and normal
    [[nodiscard]]
    static std::optional<Result> solve(
        const ColliderInterfaceType& a,
        const ColliderInterfaceType& b,
        const Simplex<VectorType>& simplex,
        const Params params = {},
        std::pmr::memory_resource& mem_resource = *std::pmr::get_default_resource()
    );
};
```

`mem_resource` backs the internal `std::pmr` containers (vertices, faces, heap, horizon map); pass a monotonic buffer to avoid heap allocations.

### Precondition

The `simplex` parameter must:
* Have exactly 4 points (`simplex.size() == 4`)
* Contain the origin (i.e., be a valid GJK result with `hit == true`)

Violating this precondition leads to undefined behavior.

---

## Collider Requirements

Any type used as `ColliderInterfaceType` must provide:

```cpp
// Type alias for the vector type (typically Vector3<float>)
using VectorType = /* ... */;

// Find the farthest point in world space along the given direction
[[nodiscard]]
VectorType find_abs_furthest_vertex_position(const VectorType& direction) const;
```

See [`ColliderInterface`](collider_interface.md).

---

## Algorithm Details

### Expanding Polytope

EPA maintains a convex polytope (polyhedron) in Minkowski difference space `A - B`. Starting from the 4-point tetrahedron (simplex from GJK), it repeatedly:

1. **Find closest face** to the origin
2. **Support query** in the direction of the face normal
3. **Expand polytope** by adding the new support point
4. **Update faces** to maintain convexity

The algorithm terminates when:
* **Convergence**: the distance from origin to polytope stops growing (within tolerance)
* **Max iterations**: safety limit reached
* **Failure cases**: degenerate polytope or numerical issues

### Minkowski Difference

Like GJK, EPA operates in Minkowski difference space where `point = a - b` for points in shapes A and B. The closest point on this polytope to the origin gives the minimum separation.

### Face Winding

Faces are stored with outward-pointing normals. The algorithm uses a priority queue to efficiently find the face closest to the origin.

---

## Vertex Type Requirements

The collider's `VectorType` (with `FloatingType = VectorType::ContainedType`) must satisfy the `EpaVector` concept:

```cpp
template<class V, class FloatingType>
concept EpaVector = requires(const V& a, const V& b, FloatingType s) {
    { a - b } -> std::same_as<V>;
    { a.cross(b) } -> std::same_as<V>;
    { a.dot(b) } -> std::same_as<FloatingType>;
    { -a } -> std::same_as<V>;
    { a * s } -> std::same_as<V>;
    { a / s } -> std::same_as<V>;
};
```

`omath::Vector3<float>` satisfies this concept.

---

## Usage Examples

### Basic EPA Usage

```cpp
using namespace omath::collision;
using namespace omath::source_engine;

// First, run GJK to detect collision
MeshCollider collider_a(mesh_a);   // omath::source_engine::MeshCollider
MeshCollider collider_b(mesh_b);

auto gjk_result = GjkAlgorithm<MeshCollider>::is_collide_with_simplex_info(
    collider_a,
    collider_b
);

if (gjk_result.hit) {
    // Collision detected, use EPA to get penetration info
    auto epa_result = Epa<MeshCollider>::solve(
        collider_a,
        collider_b,
        gjk_result.simplex
    );

    if (epa_result) {
        std::cout << "Penetration depth: " << epa_result->depth << "\n";
        std::cout << "Separation normal: "
                  << "(" << epa_result->normal.x << ", "
                  << epa_result->normal.y << ", "
                  << epa_result->normal.z << ")\n";

        // penetration_vector = normal * depth; move A or B along +/- it to separate
        // (try both signs, as the tests do, if you rely on a specific orientation)
        const Vector3<float> correction = epa_result->penetration_vector;
    }
}
```

### Custom Parameters

```cpp
// Use custom convergence settings
Epa<Collider>::Params params;
params.max_iterations = 128;    // Allow more iterations for complex shapes
params.tolerance = 1e-5f;       // Tighter tolerance for more accuracy

auto result = Epa<Collider>::solve(a, b, simplex, params);

// Optional: allocate the internal containers from your own arena
std::pmr::monotonic_buffer_resource pool{1024};
auto result2 = Epa<Collider>::solve(a, b, simplex, params, pool);
```

### Physics Integration

```cpp
void resolve_collision(PhysicsBody& body_a, PhysicsBody& body_b) {
    auto gjk_result = GjkAlgorithm<Collider>::is_collide_with_simplex_info(
        body_a.collider, body_b.collider
    );

    if (!gjk_result.hit)
        return;  // No collision

    auto epa_result = Epa<Collider>::solve(
        body_a.collider,
        body_b.collider,
        gjk_result.simplex
    );

    if (epa_result) {
        // Separate bodies
        float mass_sum = body_a.mass + body_b.mass;
        float ratio_a = body_b.mass / mass_sum;
        float ratio_b = body_a.mass / mass_sum;

        body_a.position += epa_result->normal * (epa_result->depth * ratio_a);
        body_b.position -= epa_result->normal * (epa_result->depth * ratio_b);

        // Apply collision response
        apply_impulse(body_a, body_b, epa_result->normal);
    }
}
```

---

## Performance Characteristics

* **Time complexity**: O(k × f) where k is iterations and f is faces per iteration (typically f grows slowly)
* **Space complexity**: O(n) where n is the number of polytope vertices (typically < 100)
* **Typical iterations**: 4-20 for most collisions
* **Worst case**: 64 iterations (configurable limit)

### Performance Tips

1. **Adjust max_iterations**: Balance accuracy vs. performance for your use case
2. **Tolerance tuning**: Larger tolerance = faster convergence but less accurate
3. **Shape complexity**: Simpler shapes (fewer faces) converge faster
4. **Deep penetrations**: Require more iterations; consider broad-phase separation

---

## Limitations & Edge Cases

* **Requires valid simplex**: Must be called with a 4-point simplex containing the origin (from successful GJK)
* **Convex shapes only**: Like GJK, EPA only works with convex colliders
* **Convergence failure**: Can fail for degenerate or very thin shapes (`solve` returns an empty `std::optional`). If the iteration cap is reached, the best surviving face is returned instead of failing
* **Numerical precision**: Extreme scale differences or very small shapes may cause issues
* **Deep penetration**: Very deep intersections may require many iterations or fail to converge

### Error Handling

```cpp
auto result = Epa<Collider>::solve(a, b, simplex);

if (!result) {
    // EPA failed (no surviving polytope face)
    // Fallback options:
    // 1. Use a default separation (e.g., axis between centers)
    // 2. Increase max_iterations and retry
    // 3. Log a warning and skip this collision
}
```

---

## Theory & Background

### Why EPA after GJK?

GJK determines **if** shapes intersect but doesn't compute penetration depth. EPA extends GJK's final simplex to find the exact depth and normal needed for:
* **Collision response** — separating objects realistically
* **Contact manifolds** — generating contact points for physics
* **Constraint solving** — iterative physics solvers

### Comparison with SAT

| Feature | EPA | SAT (Separating Axis Theorem) |
|---------|-----|-------------------------------|
| Works with | Any convex shape | Polytopes (faces/edges) |
| Penetration depth | Yes | Yes |
| Complexity | Iterative | Per-axis projection |
| Best for | General convex | Boxes, prisms |
| Typical speed | Moderate | Fast (few axes) |

EPA is more general; SAT is faster for axis-aligned shapes.

---

## Implementation Details

The EPA implementation in OMath:
* Uses a **priority queue** to efficiently find the closest face
* Maintains face winding for consistent normals
* Handles **edge cases**: degenerate faces, numerical instability
* Prevents infinite loops with iteration limits
* Returns detailed diagnostics (iteration count, polytope size) in `Result`
* Uses `std::pmr` containers, so the caller can supply the memory resource

---

## See Also

- [GJK Algorithm Documentation](gjk_algorithm.md) - Collision detection (required before EPA)
- [Simplex Documentation](simplex.md) - Input simplex structure
- [ColliderInterface Documentation](collider_interface.md) - Collider base class
- [MeshCollider Documentation](mesh_collider.md) - Mesh-based collider
- [Mesh Documentation](../3d_primitives/mesh.md) - Mesh primitive
- [Tutorials - Collision Detection](../tutorials.md#tutorial-4-collision-detection) - Complete collision tutorial
- [API Overview](../api_overview.md) - High-level API reference

---

*Last updated: 13 Nov 2025*
