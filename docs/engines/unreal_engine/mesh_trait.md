# `omath::unreal_engine::MeshTrait` — mesh transformation trait for Unreal Engine

> Header: `omath/engines/unreal_engine/traits/mesh_trait.hpp`
> Namespace: `omath::unreal_engine`
> Purpose: provide Unreal Engine-specific rotation matrix computation for `omath::primitives::Mesh`

---

## Summary

`MeshTrait` is a trait class that provides the `rotation_matrix` function for transforming meshes in Unreal Engine's coordinate system. It serves as a template parameter to `omath::primitives::Mesh`, enabling engine-specific rotation behavior.

---

## Coordinate System

**Unreal Engine** uses:
* **Up axis**: +Z
* **Forward axis**: +X
* **Right axis**: +Y
* **Handedness**: Left-handed
* **Rotation**: `Rz(yaw) · Ry(-pitch) · Rx(-roll)` (`mat_rotation_zyx`), i.e. roll is applied first, then pitch, then yaw

---

## API

```cpp
namespace omath::unreal_engine {

class MeshTrait final {
public:
    [[nodiscard]]
    static Mat4X4 rotation_matrix(const ViewAngles& rotation);
};

} // namespace omath::unreal_engine
```

---

## Method: `rotation_matrix`

```cpp
static Mat4X4 rotation_matrix(const ViewAngles& rotation);
```

Computes a 4×4 rotation matrix from Unreal-style Euler angles.

**Parameters**:
* `rotation` — `ViewAngles` containing pitch, yaw, and roll angles

**Returns**: 4×4 rotation matrix suitable for mesh transformation

**Implementation**: Delegates to `unreal_engine::rotation_matrix(rotation)` defined in `formulas.hpp`.

---

## Usage

### With Mesh

```cpp
using namespace omath::unreal_engine;

// Create mesh (MeshTrait is used automatically)
Mesh my_mesh(vertices, indices);

// Set rotation using ViewAngles
ViewAngles angles;
angles.pitch = PitchAngle::from_degrees(30.0f);
angles.yaw = YawAngle::from_degrees(45.0f);
angles.roll = RollAngle::from_degrees(0.0f);

my_mesh.set_rotation(angles);

// The rotation matrix is computed using MeshTrait::rotation_matrix
auto matrix = my_mesh.get_to_world_matrix();
```

---

## Rotation Conventions

Unreal uses a left-handed Z-up coordinate system:

1. **Roll** (rotation around X-axis / forward axis)
   * Range: [-180°, 180°]

2. **Pitch** (rotation around Y-axis / right axis)
   * Positive pitch looks upward
   * Range: [-90°, 90°]

3. **Yaw** (rotation around Z-axis / up axis)
   * Positive yaw rotates clockwise when viewed from above (left-handed)
   * Range: [-180°, 180°]

**Note**: `rotation_matrix` negates pitch and roll before building the matrix, so positive pitch looks up.

---

## Type Alias

```cpp
namespace omath::unreal_engine {
    // mesh.hpp
    using Mesh = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait, float>;

    // primitives.hpp
    using BoxMesh   = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait, primitives::Vertex<>,
                                       std::array<Vector3<float>, 8>, std::array<Vector3<std::uint32_t>, 12>>;
    using PlaneMesh = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait, primitives::Vertex<>,
                                       std::array<Vector3<float>, 4>, std::array<Vector3<std::uint32_t>, 2>>;
}
```

`BoxMesh` (8 vertices, 12 triangles) and `PlaneMesh` (4 vertices, 2 triangles) are fixed-size meshes declared in `primitives.hpp`. Unreal Engine has no `collider.hpp`, so there is no ready-made `MeshCollider` alias; wrap a mesh with `collision::MeshCollider<Mesh>` yourself if needed.

---

## See Also

- [Mesh Documentation](../../3d_primitives/mesh.md) - Mesh primitive
- [Formulas Documentation](formulas.md) - Unreal rotation formula
- [CameraTrait Documentation](camera_trait.md) - Camera trait

---

*Last updated: 13 Nov 2025*
