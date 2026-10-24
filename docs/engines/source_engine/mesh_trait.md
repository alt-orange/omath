# `omath::source_engine::MeshTrait` — mesh transformation trait for Source Engine

> Header: `omath/engines/source_engine/traits/mesh_trait.hpp`
> Namespace: `omath::source_engine`
> Purpose: provide Source Engine-specific rotation matrix computation for `omath::primitives::Mesh`

---

## Summary

`MeshTrait` is a trait class that provides the `rotation_matrix` function for transforming meshes in Source Engine's coordinate system. It serves as a template parameter to `omath::primitives::Mesh`, enabling engine-specific rotation behavior.

---

## Coordinate System

**Source Engine** uses:
* **Up axis**: +Z
* **Forward axis**: +X
* **Right axis**: -Y
* **Handedness**: Right-handed
* **Rotation**: `Rz(yaw) · Ry(pitch) · Rx(roll)` (`mat_rotation_zyx`)

---

## API

```cpp
namespace omath::source_engine {

class MeshTrait final {
public:
    [[nodiscard]]
    static Mat4X4 rotation_matrix(const ViewAngles& rotation);
};

// mesh.hpp
using Mesh = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait>;
// collider.hpp
using MeshCollider = collision::MeshCollider<Mesh>;
// primitives.hpp
using BoxMesh   = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait, primitives::Vertex<>,
                                   std::array<Vector3<float>, 8>, std::array<Vector3<std::uint32_t>, 12>>;
using PlaneMesh = primitives::Mesh<Mat4X4, ViewAngles, MeshTrait, primitives::Vertex<>,
                                   std::array<Vector3<float>, 4>, std::array<Vector3<std::uint32_t>, 2>>;

} // namespace omath::source_engine
```

---

## Method: `rotation_matrix`

```cpp
static Mat4X4 rotation_matrix(const ViewAngles& rotation);
```

Computes a 4×4 rotation matrix from Source Engine-style Euler angles.

**Parameters**:
* `rotation` — `ViewAngles` containing pitch, yaw, and roll angles

**Returns**: 4×4 rotation matrix suitable for mesh transformation

**Implementation**: Delegates to `source_engine::rotation_matrix(rotation)` defined in `formulas.hpp`.

---

## Usage

### With Mesh

```cpp
using namespace omath::source_engine;

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

### Direct Usage

```cpp
using namespace omath::source_engine;

ViewAngles angles;
angles.pitch = PitchAngle::from_degrees(45.0f);
angles.yaw = YawAngle::from_degrees(90.0f);
angles.roll = RollAngle::from_degrees(0.0f);

Mat4X4 rot_matrix = MeshTrait::rotation_matrix(angles);

// Use the matrix directly
Vector3<float> local_point{1, 0, 0};
auto rotated = rot_matrix * mat_column_from_vector(local_point);
```

---

## Rotation Conventions

The rotation matrix is built following Source Engine's conventions:

1. **Pitch** (rotation around the Y axis, the left/right axis)
   * **Negative** pitch looks upward (+Z direction)
   * Range: [-89°, 89°]

2. **Yaw** (rotation around Z-axis / up axis)
   * Positive yaw rotates counterclockwise when viewed from above
   * Range: [-180°, 180°]

3. **Roll** (rotation around X-axis / forward axis)
   * Range: [-180°, 180°]

**Composition**: The matrices are combined as `Rz(yaw) · Ry(pitch) · Rx(roll)`, producing a rotation that:
* First applies roll around the forward axis
* Then applies pitch around the Y axis
* Finally applies yaw around the up axis

---

## Related Functions

The trait delegates to the formula defined in `formulas.hpp`:

```cpp
[[nodiscard]]
constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;
```

See [Formulas Documentation](formulas.md) for details on the rotation matrix computation.

---

## Type Aliases

The Source Engine mesh types are pre-defined (see the API block above):

* `Mesh` (`mesh.hpp`) — `primitives::Mesh<Mat4X4, ViewAngles, MeshTrait>` with the default `primitives::Vertex<>` and `std::vector` buffers.
* `MeshCollider` (`collider.hpp`) — `collision::MeshCollider<Mesh>`, a collider over a `Mesh` for the GJK/EPA collision code.
* `BoxMesh` (`primitives.hpp`) — fixed-size mesh with 8 vertices and 12 triangles.
* `PlaneMesh` (`primitives.hpp`) — fixed-size mesh with 4 vertices and 2 triangles.

Use these aliases to ensure correct trait usage:

```cpp
using namespace omath::source_engine;

// Correct: uses Source Engine trait
Mesh my_mesh(vbo, ebo);
MeshCollider collider(my_mesh);
```

---

## Notes

* **Angle ranges**: Ensure angles are within valid ranges (pitch: [-89°, 89°], yaw/roll: [-180°, 180°])
* **Performance**: Matrix computation is O(1) with ~64 floating-point operations
* **Caching**: The mesh caches the transformation matrix; recomputed only when rotation changes
* **Compatibility**: Works with all Source Engine games (CS:GO, TF2, Portal, Half-Life 2, etc.)

---

## See Also

- [Mesh Documentation](../../3d_primitives/mesh.md) - Mesh primitive using this trait
- [MeshCollider Documentation](../../collision/mesh_collider.md) - Collision wrapper for meshes (`source_engine::MeshCollider`)
- [Formulas Documentation](formulas.md) - Source Engine rotation formula
- [CameraTrait Documentation](camera_trait.md) - Camera transformation trait
- [Constants Documentation](constants.md) - Source Engine constants
- [API Overview](../../api_overview.md) - High-level API reference

---

*Last updated: 13 Nov 2025*
