# `omath::cry_engine::MeshTrait` — mesh rotation trait

> Header: `omath/engines/cry_engine/traits/mesh_trait.hpp` (alias: `omath/engines/cry_engine/mesh.hpp`)
> Namespace: `omath::cry_engine`
> Purpose: rotation for `primitives::Mesh` in CryEngine space

---

## API

```cpp
namespace omath::cry_engine {

class MeshTrait final {
public:
  static Mat4X4 rotation_matrix(const ViewAngles& rotation); // cry_engine::rotation_matrix
};

using Mesh = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait>; // mesh.hpp

} // namespace omath::cry_engine
```

`MeshTrait::rotation_matrix` is a plain forwarder to [`cry_engine::rotation_matrix`](formulas.md#rotation-matrix). It is not
`constexpr` and not `noexcept`, unlike the free function.

Meshes rotate with the same yaw · pitch · roll order as the camera (see [Formulas](formulas.md)); roll turns around the
mesh's own forward (`+Y`) axis.

---

## Usage

`primitives::Mesh` builds its to-world matrix as `translation * MeshTrait::rotation_matrix(angles) * scale`:

```cpp
using namespace omath;

void place(cry_engine::Mesh& mesh)
{
    mesh.set_origin({10.f, 20.f, 0.f});
    mesh.set_rotation({cry_engine::PitchAngle::from_degrees(0.f), cry_engine::YawAngle::from_degrees(90.f),
                       cry_engine::RollAngle::from_degrees(0.f)});
    mesh.set_scale({2.f, 2.f, 2.f});

    const auto& to_world = mesh.get_to_world_matrix(); // cry_engine::Mat4X4, cached until a setter is called
}
```

Unlike the Serious Engine, this engine has no ready-made `MeshCollider`, `BoxMesh` or `PlaneMesh` aliases; only `Mesh`
is defined.

---

## See also

* [Formulas](formulas.md) — `rotation_matrix`
* [Constants](constants.md) — `Mat4X4` and `ViewAngles`
* [Mesh](../../3d_primitives/mesh.md)
