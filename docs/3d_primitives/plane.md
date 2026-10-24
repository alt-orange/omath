# `omath::primitives::create_plane` — Build a quad mesh (4 vertices, 2 triangles)

> Header: `omath/3d_primitives/plane.hpp`
> Namespace: `omath::primitives`
> Depends on: [`omath::primitives::Mesh`](mesh.md), `omath::Vector3<float>`
> Engine aliases: `omath::<engine>::PlaneMesh` in `omath/engines/<engine>/primitives.hpp` (frostbite, iw, opengl, serious, source, unity, unreal)

```cpp
template<class PlaneMeshType>
[[nodiscard]]
PlaneMeshType create_plane(const Vector3<float>& vertex_a,
                           const Vector3<float>& vertex_b,
                           const Vector3<float>& direction,
                           float size) noexcept;
```

---

## What it does

Creates a **quad** by extruding the edge `vertex_a -> vertex_b` along `direction`, and returns it as a `PlaneMeshType` mesh with 4 vertices and 2 triangles.

`PlaneMeshType` is a template parameter: pick the mesh type of your engine, for example `omath::opengl_engine::PlaneMesh`. It is constructed as `PlaneMeshType{std::array<Vector3<float>, 4>, std::array<Vector3<std::uint32_t>, 2>}`, so it must be a `primitives::Mesh` with those buffer types (exactly what the engine `PlaneMesh` aliases are).

Construction (from the implementation):

```text
second_vertex_a = vertex_a + direction * size
second_vertex_b = vertex_b + direction * size
vertices = { vertex_a, vertex_b, second_vertex_a, second_vertex_b }   // indices 0..3
```

* `direction` is **not** normalized or orthogonalized: the extrusion vector is `direction * size`, so its length is `|direction| * size`.
* `size` is therefore the extrusion distance (when `direction` is a unit vector), not a half-width.

---

## Parameters

* `vertex_a`, `vertex_b` — the two vertices of the base edge.
* `direction` — extrusion direction (used as given).
* `size` — scale applied to `direction` to get the opposite edge.

---

## Return

A `PlaneMeshType` mesh. Vertices are `{vertex_a, vertex_b, vertex_a + direction * size, vertex_b + direction * size}` and the element buffer holds two index triples:

```cpp
poly[0] = {1, 1, 2};
poly[1] = {0, 1, 3};
```

!!! warning "First triangle repeats an index"
    As written in the header, the first face is `{1, 1, 2}` (vertex 1 appears twice), which is a degenerate triangle, while the second is `{0, 1, 3}`. Treat the returned element buffer as a quirk of the current implementation rather than a clean two-triangle quad until this is reviewed.

---

## Example

```cpp
#include <omath/3d_primitives/plane.hpp>
#include <omath/engines/opengl_engine/primitives.hpp>

using omath::Vector3;

Vector3<float> a{ -1, 0, -1 };    // edge start
Vector3<float> b{  1, 0, -1 };    // edge end
Vector3<float> dir{ 0, 0, 1 };    // extrusion direction (+Z)
float size = 2.0f;                // extrusion distance

auto quad = omath::primitives::create_plane<omath::opengl_engine::PlaneMesh>(a, b, dir, size);

for (auto it = quad.m_element_buffer_object.cbegin(); it != quad.m_element_buffer_object.cend(); ++it) {
  const auto face = quad.make_face_in_local_space(it);   // Triangle<Vector3<float>>
  (void)face;
}
```

---

## Notes & edge cases

* **Degenerate edge**: if `vertex_a == vertex_b`, the quad collapses to a segment.
* **Collinearity**: if `direction` is parallel to `vertex_b - vertex_a`, all four vertices are collinear; no fallback is applied.
* **Winding**: if your renderer expects a specific face order, verify against the index buffer above.
* **Header cost**: `plane.hpp` includes the OpenGL engine camera and mesh headers.
