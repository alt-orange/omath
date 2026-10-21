# `omath::serious_engine::MeshTrait` — mesh rotation trait

> Header: `omath/engines/serious_engine/traits/mesh_trait.hpp`
> Namespace: `omath::serious_engine`
> Purpose: rotation for `primitives::Mesh` in Serious Engine space

---

## API

```cpp
namespace omath::serious_engine {

class MeshTrait final {
public:
  static constexpr Mat4X4 rotation_matrix(const ViewAngles& rotation) noexcept; // serious_engine::rotation_matrix
};

using Mesh         = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait>;    // mesh.hpp
using MeshCollider = collision::MeshCollider<Mesh>;                       // collider.hpp
using BoxMesh      = /* 8 vertices, 12 triangles */;                      // primitives.hpp
using PlaneMesh    = /* 4 vertices, 2 triangles */;                       // primitives.hpp

} // namespace omath::serious_engine
```

Meshes rotate with the same heading · pitch · banking order as the engine (see [Formulas](formulas.md)).

---

## See also

* [Mesh](../../3d_primitives/mesh.md)
* [Mesh Collider](../../collision/mesh_collider.md)
