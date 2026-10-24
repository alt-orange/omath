# `omath::primitives::Aabb` — axis-aligned bounding box

> Header: `omath/3d_primitives/aabb.hpp`
> Namespace: `omath::primitives`
> Template: `Aabb<Type, Up = UpAxis::Y>`
> Depends on: [`Vector3`](../linear_algebra/vector3.md)
> Purpose: min/max box aligned to the world axes, with helpers for center, extents, top/bottom anchor points, corners and overlap

---

## API

```cpp
namespace omath::primitives {

enum class UpAxis { X, Y, Z };

template<class Type, UpAxis Up = UpAxis::Y>
struct Aabb final {
  Vector3<Type> min;
  Vector3<Type> max;

  [[nodiscard]] static consteval UpAxis get_up_axis();

  [[nodiscard]] constexpr Vector3<Type> center()  const noexcept;   // (min + max) / 2
  [[nodiscard]] constexpr Vector3<Type> extents() const noexcept;   // (max - min) / 2  (half-size)
  [[nodiscard]] constexpr Vector3<Type> top()     const noexcept;   // center of the top face
  [[nodiscard]] constexpr Vector3<Type> bottom()  const noexcept;   // center of the bottom face
  [[nodiscard]] constexpr std::array<Vector3<Type>, 8> vertices() const noexcept;
  [[nodiscard]] constexpr bool is_collide(const Aabb& other) const noexcept;
};

} // namespace omath::primitives
```

`Aabb` is an aggregate: initialize it with `{min, max}`. It does not check that `min <= max`.

---

## Behavior

* **`Up`** only decides which axis `top()` and `bottom()` use. Pick the axis that is "up" in the coordinate system you are working in.
* **`top()`** returns the center of the face at `max` along the up axis; the two other coordinates come from `center()`. For `Up = Y` that is `{center.x, max.y, center.z}`.
* **`bottom()`** is the same with `min` along the up axis.
* **`extents()`** is the half-size per axis, not the full size.
* **`vertices()`** returns the 8 corners. Index bits select the side per axis: bit 0 is X, bit 1 is Y, bit 2 is Z, and a set bit means `max`. So `v[0] = min`, `v[1] = {max.x, min.y, min.z}`, `v[7] = max`.
* **`is_collide(other)`** is an inclusive interval overlap test on all three axes, so boxes that only touch count as colliding. It is boolean only; there is no penetration depth (use [GJK](../collision/gjk_algorithm.md) / [EPA](../collision/epa_algorithm.md) for that).

---

## Where it is used

* [`Mesh::local_bounds()`](mesh.md) returns the mesh's bounds in its own local frame, meant as a broadphase reject.
* [`LineTracer::get_ray_hit_point(ray, aabb)`](../collision/line_tracer.md) intersects a ray with the box (slab method) and returns the hit point, or `ray.end` on a miss.
* `Camera::is_aabb_culled_by_frustum(aabb)` returns `true` when the box is entirely outside the view frustum; see [Camera](../projection/camera.md).

---

## Usage

```cpp
#include <omath/3d_primitives/aabb.hpp>

using Box = omath::primitives::Aabb<float, omath::primitives::UpAxis::Z>;

constexpr Box player{{-16.f, -16.f, 0.f}, {16.f, 16.f, 72.f}};

constexpr auto head = player.top();      // (0, 0, 72)
constexpr auto feet = player.bottom();   // (0, 0, 0)
constexpr auto half = player.extents();  // (16, 16, 36)

constexpr Box wall{{10.f, -100.f, 0.f}, {20.f, 100.f, 200.f}};
static_assert(player.is_collide(wall));
```

---

## Notes

* For a box that is rotated with its entity, use [`Obb`](obb.md) instead.
* The header references `std::unreachable()` in a branch that cannot be taken but does not include `<utility>` itself; include it before this header if your toolchain complains.

---

## See also

* [`Obb`](obb.md)
* [`create_box`](box.md) — builds a box mesh as 12 triangles (a different helper from `Aabb`)
* [Line tracer](../collision/line_tracer.md)
