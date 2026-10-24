# `omath::primitives::Obb` — oriented bounding box

> Header: `omath/3d_primitives/obb.hpp`
> Namespace: `omath::primitives`
> Template: `Obb<Type>` with `std::is_floating_point_v<Type>`
> Depends on: [`Vector3`](../linear_algebra/vector3.md)
> Purpose: rectangular cuboid defined by a center, three orthonormal local axes and a half-size along each axis

---

## API

```cpp
namespace omath::primitives {

template<class Type>
requires std::is_floating_point_v<Type>
struct Obb final {
  Vector3<Type> center;
  Vector3<Type> axis_x;        // local axes, expected orthonormal
  Vector3<Type> axis_y;
  Vector3<Type> axis_z;
  Vector3<Type> half_extents;  // half-size along axis_x / axis_y / axis_z

  [[nodiscard]] constexpr std::array<Vector3<Type>, 8> vertices() const noexcept;
};

} // namespace omath::primitives
```

`Obb` is an aggregate; the member order above is the initialization order.

---

## Behavior

* **`vertices()`** returns the 8 corners `center ± axis_x*half_extents.x ± axis_y*half_extents.y ± axis_z*half_extents.z`. The ordering matches [`Aabb::vertices()`](aabb.md): bit 0 selects `+axis_x`, bit 1 selects `+axis_y`, bit 2 selects `+axis_z`. So `v[0]` is `center - ex - ey - ez` and `v[7]` is `center + ex + ey + ez`.
* An axis-aligned `Obb` (axes `{1,0,0}`, `{0,1,0}`, `{0,0,1}`) gives the same corners as an `Aabb` with `min = center - half_extents` and `max = center + half_extents`.
* The type is data only: it has no `center()`, overlap or containment helpers. It is consumed by the algorithms below.

!!! warning "Axes must be orthonormal"
    Nothing validates or normalizes the axes. The ray tracer and the frustum test project onto them with dot products and rely on unit length and mutual perpendicularity. Build them by rotating the unit X, Y and Z vectors with the same rotation (for example [`Quaternion::rotate`](../linear_algebra/quaternion.md)) rather than by hand when you can.

---

## Where it is used

* [`LineTracer::get_ray_hit_point(ray, obb)`](../collision/line_tracer.md) transforms the ray into the box frame and runs a slab test. It returns the hit point in world space, or `ray.end` on a miss. A ray that starts inside the box returns `ray.start`.
* `Camera::is_obb_culled_by_frustum(obb)` returns `true` when the box is entirely outside the view frustum; see [Camera](../projection/camera.md).

---

## Usage

```cpp
#include <omath/3d_primitives/obb.hpp>
#include <cmath>
#include <numbers>

using ObbF = omath::primitives::Obb<float>;

// A 2 x 2 x 4 box (half-extents 1, 1, 2) turned 30 degrees around Z
const float a = std::numbers::pi_v<float> / 6.f;
const float c = std::cos(a), s = std::sin(a);

const ObbF box{
    /*center*/ {10.f, 0.f, 0.f},
    /*axis_x*/ {c, s, 0.f},
    /*axis_y*/ {-s, c, 0.f},
    /*axis_z*/ {0.f, 0.f, 1.f},
    /*half_extents*/ {1.f, 1.f, 2.f},
};

const auto corners = box.vertices();
```

---

## See also

* [`Aabb`](aabb.md)
* [`create_box`](box.md) — builds a box mesh as 12 triangles
* [Line tracer](../collision/line_tracer.md)
* [Camera](../projection/camera.md)
