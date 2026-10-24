# `omath::collision::MeshCollider` — Convex hull collider for meshes

> Header: `omath/collision/mesh_collider.hpp`
> Namespace: `omath::collision`
> Depends on: `omath::primitives::Mesh`, `omath::collision::ColliderInterface`, `omath::Vector3<T>`
> Purpose: wrap a mesh to provide collision detection support for GJK/EPA

---

## Overview

`MeshCollider` wraps a `Mesh` object to provide the **support function** interface required by the GJK and EPA collision detection algorithms. The support function finds the vertex of the mesh farthest along a given direction, which is essential for constructing Minkowski difference simplices.

**Important**: `MeshCollider` assumes the mesh represents a **convex hull**. For non-convex shapes, you must either:
* Decompose into convex parts
* Use the convex hull of the mesh
* Use a different collision detection algorithm

---

## Template Declaration

```cpp
template<class MeshType>
class MeshCollider final : public ColliderInterface<typename MeshType::VertexType::VectorType>;
```

### MeshType Requirements

`MeshType` must be an instantiation of `omath::primitives::Mesh` (or provide the same members). `MeshCollider` uses:

* `MeshType::VertexType` — the vertex element type; it must have a `position` member and a nested `VectorType` alias (e.g. `primitives::Vertex<>`)
* `m_vertex_buffer` — a range of those vertices
* `get_to_world_matrix()`, `vertex_position_to_world_space(...)`, `get_origin()`, `set_origin(...)`

Ready-made aliases (header `omath/engines/<engine>/collider.hpp`):
* `omath::source_engine::MeshCollider`
* `omath::serious_engine::MeshCollider`

For any other engine instantiate it yourself: `MeshCollider<omath::unity_engine::Mesh>` etc. (subject to that `Mesh` alias being valid for your vertex type).

---

## Type Aliases

```cpp
using VertexType = MeshType::VertexType;
using VectorType = MeshType::VertexType::VectorType;
```

* `VertexType` — vertex element of the mesh's vertex buffer
* `VectorType` — 3D vector type (this is also the `VectorType` seen by [`GjkAlgorithm`](gjk_algorithm.md) / [`Epa`](epa_algorithm.md))

---

## Constructor

```cpp
explicit MeshCollider(MeshType mesh);
```

Creates a collider from a mesh. The mesh is **moved** into the collider, so pass by value:

```cpp
omath::source_engine::Mesh my_mesh = /* ... */;
omath::source_engine::MeshCollider collider(std::move(my_mesh));
```

---

## Methods

### `find_abs_furthest_vertex_position`

```cpp
[[nodiscard]]
VectorType find_abs_furthest_vertex_position(const VectorType& direction) const override;
```

Finds the vertex farthest along `direction` (given in **world space**) and returns it in **world space**. This is the primary method used by GJK/EPA.

**Steps**:
1. Transform `direction` into the mesh's local space once, using the transpose of the upper-left 3x3 block of the to-world matrix
2. Linear search (`std::ranges::max_element`) over `m_vertex_buffer` comparing `position.dot(d_local)` — O(n) in the vertex count
3. Transform the winning vertex to world space with `mesh.vertex_position_to_world_space()`

The local-space search avoids a full 4x4 multiply per vertex.

**Usage in GJK**:
```cpp
// GJK support function for Minkowski difference
VectorType support = collider_a.find_abs_furthest_vertex_position(direction)
                   - collider_b.find_abs_furthest_vertex_position(-direction);
```

### Origin and mesh access

```cpp
[[nodiscard]] const VectorType& get_origin() const override;   // mesh origin
void set_origin(const VectorType& new_origin) override;        // moves the mesh

[[nodiscard]] const MeshType& get_mesh() const;
[[nodiscard]] MeshType&       get_mesh();
```

`get_origin()` is what `GjkAlgorithm` uses to seed its initial search direction.

> `find_furthest_vertex` (local-space query) is **private**; use `find_abs_furthest_vertex_position`.

---

## Usage Examples

### Basic Collision Detection

```cpp
using namespace omath::collision;
using namespace omath::source_engine;

// Create meshes with vertex data (the default Mesh vertex type is primitives::Vertex<>)
using omath::primitives::Vertex;
std::vector<Vertex<>> vbo_a = {
    {{-1, -1, -1}, {}, {}}, {{1, -1, -1}, {}, {}}, {{1, 1, -1}, {}, {}}, {{-1, 1, -1}, {}, {}},
    {{-1, -1,  1}, {}, {}}, {{1, -1,  1}, {}, {}}, {{1, 1,  1}, {}, {}}, {{-1, 1,  1}, {}, {}}
};
std::vector<Vector3<std::uint32_t>> ebo_a = /* face indices (not used by the collider) */ {};

Mesh mesh_a(vbo_a, ebo_a);
mesh_a.set_origin({0, 0, 0});

Mesh mesh_b(vbo_b, ebo_b);
mesh_b.set_origin({5, 0, 0});  // Positioned away from mesh_a

// Wrap in colliders
MeshCollider collider_a(std::move(mesh_a));
MeshCollider collider_b(std::move(mesh_b));

// Run GJK
if (GjkAlgorithm<MeshCollider>::is_collide(collider_a, collider_b)) {
    std::cout << "Collision detected!\n";
}
```

### With EPA for Penetration Depth

```cpp
auto gjk_result = GjkAlgorithm<MeshCollider>::is_collide_with_simplex_info(
    collider_a, collider_b
);

if (gjk_result.hit) {
    auto epa_result = Epa<MeshCollider>::solve(
        collider_a, collider_b, gjk_result.simplex
    );

    if (epa_result) {
        std::cout << "Penetration: " << epa_result->depth << " units\n";
        std::cout << "Normal: " << epa_result->normal << "\n";
    }
}
```

### Custom Mesh Creation

```cpp
// Create a simple box mesh
using omath::primitives::Vertex;
std::vector<Vertex<>> box_vertices = {
    {{-0.5f, -0.5f, -0.5f}, {}, {}}, {{ 0.5f, -0.5f, -0.5f}, {}, {}},
    {{ 0.5f,  0.5f, -0.5f}, {}, {}}, {{-0.5f,  0.5f, -0.5f}, {}, {}},
    {{-0.5f, -0.5f,  0.5f}, {}, {}}, {{ 0.5f, -0.5f,  0.5f}, {}, {}},
    {{ 0.5f,  0.5f,  0.5f}, {}, {}}, {{-0.5f,  0.5f,  0.5f}, {}, {}}
};

std::vector<Vector3<std::uint32_t>> box_indices = {
    {0, 1, 2}, {0, 2, 3},  // Front face
    {4, 6, 5}, {4, 7, 6},  // Back face
    {0, 4, 5}, {0, 5, 1},  // Bottom face
    {2, 6, 7}, {2, 7, 3},  // Top face
    {0, 3, 7}, {0, 7, 4},  // Left face
    {1, 5, 6}, {1, 6, 2}   // Right face
};

using namespace omath::source_engine;
Mesh box_mesh(box_vertices, box_indices);
box_mesh.set_origin({10, 0, 0});
box_mesh.set_scale({2, 2, 2});

MeshCollider box_collider(std::move(box_mesh));
```

### Oriented Collision

```cpp
// Create rotated mesh
Mesh mesh(vertices, indices);
mesh.set_origin({5, 5, 5});
mesh.set_scale({1, 1, 1});

// Set rotation (engine-specific angles)
ViewAngles rotation;
rotation.pitch = PitchAngle::from_degrees(45.0f);
rotation.yaw = YawAngle::from_degrees(30.0f);
mesh.set_rotation(rotation);

// Collider automatically handles transformation
MeshCollider collider(std::move(mesh));

// Support function returns world-space vertices
auto support = collider.find_abs_furthest_vertex_position({0, 1, 0});
```

---

## Performance Considerations

### Linear Search

`find_abs_furthest_vertex_position` performs a **linear search** through all vertices:
* **Time complexity**: O(n) per support query
* **GJK iterations**: ~10-20 support queries per collision test
* **Total cost**: O(k × n) where k is GJK iterations

For meshes with many vertices (>1000), consider:
* Using simpler proxy geometry (bounding box, convex hull with fewer vertices)
* Pre-computing hierarchical structures
* Using specialized collision shapes when possible

### Caching Opportunities

The implementation uses `std::ranges::max_element`, which is cache-friendly for contiguous vertex buffers. For optimal performance:
* Store vertices contiguously in memory
* Avoid pointer-based or scattered vertex storage
* Consider SoA (Structure of Arrays) layout for SIMD optimization

### World Space Transformation

The support query transforms the **direction** into local space once (3x3 transpose product) and only transforms the single winning vertex to world space via `vertex_position_to_world_space`:
* **Cost**: O(n) dot products plus one 4x4 multiply per query
* **Optimization**: The mesh caches its to-world matrix
* **Update cost**: Only recomputed when origin/rotation/scale changes

---

## Limitations & Edge Cases

### Convex Hull Requirement

**Critical**: GJK/EPA only work with **convex shapes**. If your mesh is concave:

#### Option 1: Convex Decomposition
```cpp
// Decompose concave mesh into convex parts
std::vector<Mesh> convex_parts = decompose_mesh(concave_mesh);

for (const auto& part : convex_parts) {
    MeshCollider collider(part);   // omath::source_engine::MeshCollider
    // Test each part separately
}
```

#### Option 2: Use Convex Hull
```cpp
// Compute convex hull of vertices
auto hull_vertices = compute_convex_hull(mesh.m_vertex_buffer);
Mesh hull_mesh(hull_vertices, hull_indices);
MeshCollider collider(std::move(hull_mesh));
```

#### Option 3: Different Algorithm
Use triangle-based collision (e.g., LineTracer) for true concave support.

### Empty Mesh

Behavior is undefined if `m_vertex_buffer` is empty. Always ensure:
```cpp
assert(!mesh.m_vertex_buffer.empty());
MeshCollider collider(std::move(mesh));
```

### Degenerate Meshes

* **Single vertex**: Treated as a point (degenerates to sphere collision)
* **Two vertices**: Line segment (may cause GJK issues)
* **Coplanar vertices**: Flat mesh; EPA may have convergence issues

**Recommendation**: Use at least 4 non-coplanar vertices for robustness.

---

## Coordinate Systems

`MeshCollider` supports different engine coordinate systems through the `MeshTrait`:

| Engine | Up Axis | Handedness | Rotation Order |
|--------|---------|------------|----------------|
| Source Engine | Z | Right-handed | Pitch/Yaw/Roll |
| Unity | Y | Left-handed | Pitch/Yaw/Roll |
| Unreal | Z | Left-handed | Roll/Pitch/Yaw |
| Frostbite | Y | Right-handed | Pitch/Yaw/Roll |
| IW Engine | Z | Right-handed | Pitch/Yaw/Roll |
| OpenGL | Y | Right-handed | Pitch/Yaw/Roll |

The `vertex_position_to_world_space` method handles these differences transparently.

---

## Advanced Usage

### Custom Support Function

For specialized collision shapes, derive from `ColliderInterface`:

```cpp
class SphereCollider final : public omath::collision::ColliderInterface<Vector3<float>> {
public:
    Vector3<float> center;
    float radius;

    Vector3<float> find_abs_furthest_vertex_position(const Vector3<float>& direction) const override {
        return center + direction.normalized() * radius;
    }
    const Vector3<float>& get_origin() const override { return center; }
    void set_origin(const Vector3<float>& new_origin) override { center = new_origin; }
};

// Use with GJK/EPA
auto result = GjkAlgorithm<SphereCollider>::is_collide(sphere_a, sphere_b);
```

### Debugging Support Queries

```cpp
// MeshCollider is `final`, so wrap it instead of deriving from it:
template<class MeshType>
struct DebugCollider {
    using VectorType = typename MeshCollider<MeshType>::VectorType;
    MeshCollider<MeshType> inner;

    VectorType find_abs_furthest_vertex_position(const VectorType& direction) const {
        auto result = inner.find_abs_furthest_vertex_position(direction);
        std::cout << "Support query: direction=" << direction
                  << " -> vertex=" << result << "\n";
        return result;
    }
    const VectorType& get_origin() const { return inner.get_origin(); }
};
```

---

## See Also

- [ColliderInterface Documentation](collider_interface.md) - Base class of `MeshCollider`
- [GJK Algorithm Documentation](gjk_algorithm.md) - Uses `MeshCollider` for collision detection
- [EPA Algorithm Documentation](epa_algorithm.md) - Uses `MeshCollider` for penetration depth
- [Simplex Documentation](simplex.md) - Data structure used by GJK
- [Mesh Documentation](../3d_primitives/mesh.md) - Underlying mesh primitive
- [Tutorials - Collision Detection](../tutorials.md#tutorial-4-collision-detection) - Complete collision tutorial

---

*Last updated: 13 Nov 2025*
