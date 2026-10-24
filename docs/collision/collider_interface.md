# `omath::collision::ColliderInterface` — support-function contract for GJK / EPA

> Header: `omath/collision/collider_interface.hpp`
> Namespace: `omath::collision`
> Template: `ColliderInterface<VecType = Vector3<float>>`
> Depends on: [`Vector3`](../linear_algebra/vector3.md)
> Purpose: abstract base that gives the [GJK](gjk_algorithm.md) and [EPA](epa_algorithm.md) algorithms a world-space support function and an origin

---

## API

```cpp
namespace omath::collision {

template<class VecType = Vector3<float>>
class ColliderInterface {
public:
  using VectorType = VecType;
  virtual ~ColliderInterface() = default;

  // Support function: the world-space point of the shape that lies furthest along `direction`.
  [[nodiscard]]
  virtual VectorType find_abs_furthest_vertex_position(const VectorType& direction) const = 0;

  [[nodiscard]] virtual const VectorType& get_origin() const = 0;
  virtual void set_origin(const VectorType& new_origin) = 0;
};

} // namespace omath::collision
```

---

## Behavior

* **`find_abs_furthest_vertex_position(direction)`** must return a point **in world space** (hence `abs`). `direction` is also given in world space and need not be normalized. For a convex shape the result is a point on its surface that maximizes the dot product with `direction`.
* **`get_origin()` / `set_origin()`** read and move the shape's origin. `set_origin` is expected to move the whole shape, so later support queries reflect the new position.
* **`VectorType`** is the alias GJK and EPA read to know which vector type to work in.

### How the algorithms use it

[`GjkAlgorithm<ColliderInterfaceType>`](gjk_algorithm.md) builds the Minkowski-difference support point as

```cpp
a.find_abs_furthest_vertex_position(dir) - b.find_abs_furthest_vertex_position(-dir)
```

and, when `get_origin()` is available, seeds its first search direction with `b.get_origin() - a.get_origin()` (falling back to `(1, 0, 0)` if the origins coincide). [`Epa`](epa_algorithm.md) uses the same support function and reads `ColliderInterfaceType::VectorType`.

Both algorithms are templates on the collider type, so a type does not have to derive from `ColliderInterface` to be usable; it only has to provide `VectorType` and `find_abs_furthest_vertex_position` (and optionally `get_origin`). Deriving from the interface simply documents and enforces the three functions at compile time.

---

## Provided implementation

[`MeshCollider<MeshType>`](mesh_collider.md) derives from `ColliderInterface<typename MeshType::VertexType::VectorType>` and implements the three functions on top of a [`Mesh`](../3d_primitives/mesh.md). It is a convex-hull collider, so the mesh must be convex.

Engines ship ready-made aliases so you do not spell the mesh type yourself:

| Header | Alias |
|---|---|
| `omath/engines/source_engine/collider.hpp` | `omath::source_engine::MeshCollider = collision::MeshCollider<source_engine::Mesh>` |
| `omath/engines/serious_engine/collider.hpp` | `omath::serious_engine::MeshCollider = collision::MeshCollider<serious_engine::Mesh>` |

See [source_engine mesh trait](../engines/source_engine/mesh_trait.md) and [serious_engine mesh trait](../engines/serious_engine/mesh_trait.md) for the mesh each alias wraps.

---

## Usage

A custom collider (a sphere) plugged into GJK:

```cpp
#include <omath/collision/collider_interface.hpp>
#include <omath/collision/gjk_algorithm.hpp>

using omath::Vector3;

class SphereCollider final : public omath::collision::ColliderInterface<Vector3<float>> {
public:
  SphereCollider(const Vector3<float>& center, float radius): m_center(center), m_radius(radius) {}

  Vector3<float> find_abs_furthest_vertex_position(const Vector3<float>& direction) const override
  {
    return m_center + direction.normalized() * m_radius;
  }

  const Vector3<float>& get_origin() const override { return m_center; }
  void set_origin(const Vector3<float>& new_origin) override { m_center = new_origin; }

private:
  Vector3<float> m_center;
  float m_radius;
};

SphereCollider a({0.f, 0.f, 0.f}, 1.f);
SphereCollider b({1.5f, 0.f, 0.f}, 1.f);

const bool hit = omath::collision::GjkAlgorithm<SphereCollider>::is_collide(a, b);
```

---

## Notes

* The interface is virtual, so queries go through a vtable. The algorithms are templated on the concrete collider type, which lets the compiler devirtualize when the type is `final`.
* A zero-length `direction` has no meaningful answer for most shapes; the example above would return NaN, so guard against it in your own support function if you call it directly.
* Support points must be consistent: querying the same direction twice without moving the shape has to return the same point, or GJK may not converge.

---

## See also

* [`MeshCollider`](mesh_collider.md)
* [GJK algorithm](gjk_algorithm.md)
* [EPA algorithm](epa_algorithm.md)
* [Simplex](simplex.md)
