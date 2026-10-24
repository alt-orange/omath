# `omath::primitives::create_box` — Build an oriented box mesh (8 vertices, 12 triangles)

> Header: `omath/3d_primitives/box.hpp` (declares `create_box`)
> Namespace: `omath::primitives`
> Depends on: [`omath::primitives::Mesh`](mesh.md), `omath::Vector3<float>`
> Engine aliases: `omath::<engine>::BoxMesh` in `omath/engines/<engine>/primitives.hpp` (frostbite, iw, opengl, serious, source, unity, unreal)

```cpp
template<class BoxMeshType>
[[nodiscard]]
BoxMeshType create_box(const Vector3<float>& top,
                       const Vector3<float>& bottom,
                       const Vector3<float>& dir_forward,
                       const Vector3<float>& dir_right,
                       float ratio = 4.f) noexcept;
```

---

## What it does

Builds a **rectangular cuboid ("box")** from a central axis (`bottom` -> `top`) and two orientation directions, and returns it as a `BoxMeshType` mesh with **8 vertices and 12 triangles** (2 per face x 6 faces).

`BoxMeshType` is a template parameter: pick the mesh type of your engine, for example `omath::opengl_engine::BoxMesh`. It is constructed as `BoxMeshType{std::array<Vector3<float>, 8>, std::array<Vector3<std::uint32_t>, 12>}`, so it must be a `primitives::Mesh` whose vertex buffer is `std::array<Vector3<float>, 8>` and whose element buffer is `std::array<Vector3<std::uint32_t>, 12>` (exactly what the engine `BoxMesh` aliases are).

Sizing rule (from the implementation):

> `height = |top - bottom|`, `side_size = height / ratio`.
> Each corner is `center +/- dir_forward * side_size +/- dir_right * side_size`, where `center` is `bottom` for the lower four and `top` for the upper four.

With unit-length, orthogonal `dir_forward` / `dir_right` the cross-section is therefore a square of side `2 * side_size = 2 * height / ratio`.

---

## Parameters

* `top`
  Center of the **top face**.

* `bottom`
  Center of the **bottom face**.

* `dir_forward`
  Direction that orients the box around its axis; also scales the extent (see above). Use a **unit** vector perpendicular to `top - bottom`.

* `dir_right`
  Second orientation direction; use a **unit** vector perpendicular to both `dir_forward` and `top - bottom`.

* `ratio` (default `4.0f`)
  Divisor applied to the height to get the half-extent. Larger values give a thinner box.

!!! note "No re-orthonormalization"
    The directions are used as given: they are neither normalized nor made orthogonal, so non-unit or skewed inputs produce a scaled or skewed box.

---

## Return value

A `BoxMeshType` mesh. Vertex layout (index -> corner):

| Index | Corner |
|---|---|
| 0 | front-right-bottom |
| 1 | front-left-bottom |
| 2 | back-right-bottom |
| 3 | back-left-bottom |
| 4 | front-right-top |
| 5 | front-left-top |
| 6 | back-right-top |
| 7 | back-left-top |

where front/back is `+/- dir_forward` and right/left is `+/- dir_right`. The 12 triangles are stored in the mesh's element buffer (`m_element_buffer_object`) as index triples: bottom (2), top (2), front (2), right (2), back (2), left (2).

---

## Example

```cpp
#include <omath/3d_primitives/box.hpp>
#include <omath/engines/opengl_engine/primitives.hpp>

using omath::Vector3;

// Axis from bottom to top (height 2)
Vector3<float> bottom{0, 0, 0};
Vector3<float> top   {0, 2, 0};

// Orientation around the axis
Vector3<float> forward{0, 0, 1};
Vector3<float> right  {1, 0, 0};

// Ratio 4 -> half-extent = height / 4 = 0.5
auto box = omath::primitives::create_box<omath::opengl_engine::BoxMesh>(top, bottom, forward, right, 4.0f);

// The mesh can be traced, transformed or used as a collider source
for (auto it = box.m_element_buffer_object.cbegin(); it != box.m_element_buffer_object.cend(); ++it) {
  const auto face = box.make_face_in_local_space(it);   // Triangle<Vector3<float>>
  auto n = face.calculate_normal();
  (void)n;
}
```

See `tests/general/unit_test_primitive_box.cpp` for use with [`LineTracer`](../collision/line_tracer.md).

---

## Usage notes & pitfalls

* **Degenerate axis**: If `top == bottom` the height is zero and all corners collapse onto the axis. Guard against this.
* **Directions**: Pass unit, mutually perpendicular directions (see the note above); the function does not fix them up.
* **Winding**: The index order is fixed by the implementation; if your renderer or collision code expects a particular winding, verify with a unit test.
* **Header cost**: `box.hpp` includes the OpenGL engine camera and mesh trait headers.

---

## See also

* [`omath::primitives::Mesh`](mesh.md) (transform, `make_face_in_local_space`, `make_face_in_world_space`)
* [`omath::Triangle`](../linear_algebra/triangle.md) (vertex utilities: normals, centroid, etc.)
* [`omath::Vector3`](../linear_algebra/vector3.md) (geometry operations used by the construction)
