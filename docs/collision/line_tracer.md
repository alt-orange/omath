# `omath::collision::Ray` & `LineTracer` — Ray intersection (triangle, AABB, OBB, mesh)

> Header: `omath/collision/line_tracer.hpp` (includes `omath/3d_primitives/aabb.hpp`, `omath/3d_primitives/obb.hpp`, `omath/linear_algebra/triangle.hpp`, `omath/linear_algebra/vector3.hpp`)
> Namespace: `omath::collision`
> Templates: `Ray<T = Vector3<float>>`, `LineTracer<RayType = Ray<>>`
> Algorithms: **Möller–Trumbore** (triangle), **slab method** (AABB / OBB), per-face trace in local space (mesh); no allocation

---

## Overview

These types provide a minimal, fast path to compute intersections between a **ray or line segment** and a **triangle, AABB, OBB or mesh**:

* `Ray` — start/end points plus a flag to treat the ray as **infinite** (half-line) or a **finite segment**.
* `LineTracer` — static helpers (all `constexpr`, `noexcept`):

    * `get_ray_hit_point(ray, shape)` → the hit point, or **`ray.end` when there is no hit**. Overloads exist for `Triangle`, `primitives::Aabb`, `primitives::Obb` and any mesh type (see below).
    * `can_trace_line(ray, triangle)` → `true` if the line is **unobstructed**, i.e. it does not hit the triangle (see the behavior notes below).

---

## `Ray`

```cpp
template<class T = Vector3<float>>
class Ray final {
public:
  using VectorType = T;
  VectorType start;                 // ray origin
  VectorType end;                   // end point (for finite segment) or a point along the direction
  bool       infinite_length = false;

  [[nodiscard]] constexpr VectorType direction_vector() const noexcept;             // end - start
  [[nodiscard]] constexpr VectorType direction_vector_normalized() const noexcept;  // direction_vector().normalized()
};
```

### Semantics

* **Direction**: `direction_vector() == end - start`.
  The normalized variant returns a unit vector (or the zero vector if the direction length is zero).
* **Extent**:

    * `infinite_length == true` → treat as a **semi-infinite ray** from `start` along `direction`.
    * `infinite_length == false` → treat as a **closed segment** from `start` to `end`.

> Tip: For an infinite ray that points along some vector `d`, set `end = start + d`.

---

## `LineTracer`

```cpp
template<class RayType = Ray<>>
class LineTracer final {
  using TriangleType = Triangle<typename RayType::VectorType>;
  using AABBType     = primitives::Aabb<typename RayType::VectorType::ContainedType>;
  using OBBType      = primitives::Obb<typename RayType::VectorType::ContainedType>;

public:
  LineTracer() = delete;

  [[nodiscard]]
  constexpr static bool can_trace_line(const RayType& ray, const TriangleType& triangle) noexcept;

  // Triangle: Möller–Trumbore
  [[nodiscard]]
  constexpr static auto get_ray_hit_point(const RayType& ray, const TriangleType& triangle) noexcept;

  // Axis-aligned box: slab method
  [[nodiscard]]
  constexpr static auto get_ray_hit_point(const RayType& ray, const AABBType& aabb) noexcept;

  // Oriented box: slab method in the box's local frame
  [[nodiscard]]
  constexpr static auto get_ray_hit_point(const RayType& ray, const OBBType& obb) noexcept;

  // Mesh: nearest hit over all faces, traced in the mesh's local space
  template<class MeshType>
  [[nodiscard]]
  constexpr static auto get_ray_hit_point(const RayType& ray, const MeshType& mesh) noexcept;
};
```

### Behavior & contract

* **No-hit sentinel**: every `get_ray_hit_point` overload returns `ray.end` when nothing is hit; compare the result against `ray.end` to detect a miss. There is no precondition to satisfy first.
* **`can_trace_line`**: returns `get_ray_hit_point(ray, triangle) == ray.end`, i.e. it is `true` when **no** hit was found (the line is *unobstructed* by that triangle) — the name reads as "can the line travel to its end". Use `get_ray_hit_point(...) != ray.end` to test for a hit.
* **Triangle winding**: Standard Möller–Trumbore works with either winding; no backface culling is implied here.
* **Finite vs. infinite**: for finite segments the hit parameter must lie within `(0, 1)` of `start -> end`; for `infinite_length` rays only the forward direction is checked.
* **AABB / OBB**: a ray that starts inside the box returns `ray.start`; a box entirely behind the origin, or beyond the end of a finite segment, is a miss.
* **Mesh**: the ray is folded into the mesh's local space once (via the inverse of `mesh.get_to_world_matrix()`), each face from `mesh.make_face_in_local_space(...)` is tested, and the nearest hit is returned in world space. If the world matrix is not invertible (e.g. zero scale) it returns `ray.end`. `MeshType` is typically a `primitives::Mesh`.
* **Degenerate inputs**: A zero-length ray or degenerate triangle yields **no hit** under typical Möller–Trumbore tolerances.

---

## Quick examples

### 1) Segment vs triangle

```cpp
using omath::Vector3;
using omath::Triangle;
using omath::collision::Ray;
using omath::collision::LineTracer;

Triangle<Vector3<float>> tri(
  Vector3<float>{0, 0, 0},
  Vector3<float>{1, 0, 0},
  Vector3<float>{0, 1, 0}
);

Ray<> seg;
seg.start = {0.25f, 0.25f, 1.0f};
seg.end   = {0.25f, 0.25f,-1.0f};
seg.infinite_length = false; // finite segment

const auto hit = LineTracer<>::get_ray_hit_point(seg, tri);
if (hit != seg.end) {
  // hit is the intersection point (0.25, 0.25, 0)
}
```

### 2) Infinite ray

```cpp
Ray<> ray;
ray.start = {0.5f, 0.5f,  1.0f};
ray.end   = ray.start + Vector3<float>{0, 0, -1}; // direction only
ray.infinite_length = true;

const bool hits = LineTracer<>::get_ray_hit_point(ray, tri) != ray.end;
```

### 3) Ray vs AABB

```cpp
omath::primitives::Aabb<float> box{{-1, -1, -1}, {1, 1, 1}};
Ray<> r{.start = {0, 0, -5}, .end = {0, 0, 5}};

const auto hit = LineTracer<>::get_ray_hit_point(r, box);   // (0, 0, -1); equals r.end on a miss
```

---

## Notes & edge cases

* **Normalization**: `direction_vector_normalized()` returns the zero vector for a zero-length direction (safe, but unusable for tracing).
* **Precision**: The triangle test uses `std::numeric_limits<float>::epsilon()` thresholds to reject nearly parallel cases and on the barycentric bounds; results near edges can be sensitive to floating-point error.
* **Hit location**: The point returned for a triangle lies on the triangle plane and within its area.

---

## API summary

```cpp
namespace omath::collision {

template<class T = Vector3<float>>
class Ray final {
public:
  using VectorType = T;
  VectorType start, end;
  bool infinite_length = false;

  [[nodiscard]] constexpr VectorType direction_vector() const noexcept;
  [[nodiscard]] constexpr VectorType direction_vector_normalized() const noexcept;
};

template<class RayType = Ray<>>
class LineTracer final {
public:
  LineTracer() = delete;

  [[nodiscard]] constexpr static bool can_trace_line(const RayType&, const Triangle<typename RayType::VectorType>&) noexcept;

  // each returns ray.end on a miss
  [[nodiscard]] constexpr static auto get_ray_hit_point(const RayType&, const Triangle<typename RayType::VectorType>&) noexcept;
  [[nodiscard]] constexpr static auto get_ray_hit_point(const RayType&, const primitives::Aabb<typename RayType::VectorType::ContainedType>&) noexcept;
  [[nodiscard]] constexpr static auto get_ray_hit_point(const RayType&, const primitives::Obb<typename RayType::VectorType::ContainedType>&) noexcept;
  template<class MeshType>
  [[nodiscard]] constexpr static auto get_ray_hit_point(const RayType&, const MeshType&) noexcept;
};

} // namespace omath::collision
```

---

## Implementation hints (if you extend it)

* Expose a variant that returns **barycentric coordinates** `(u, v, w)` alongside the hit point to support texture lookup or edge tests.
* Provide an overload returning `std::optional<Vector3<float>>` (or `expected`) instead of the `ray.end` sentinel.
* If you need backface culling, add a flag or dedicated function (reject hits where the signed distance is negative with respect to triangle normal).

---

## See Also

- [Plane Documentation](../3d_primitives/plane.md) - Ray-plane intersection
- [Box Documentation](../3d_primitives/box.md) - Box mesh builder
- [Mesh Documentation](../3d_primitives/mesh.md) - Meshes traced by `LineTracer`
- [Triangle Documentation](../linear_algebra/triangle.md) - Triangle primitives
- [Tutorials - Collision Detection](../tutorials.md#tutorial-4-collision-detection) - Complete collision tutorial
- [Getting Started Guide](../getting_started.md) - Quick start with OMath

---

*Last updated: 1 Nov 2025*
