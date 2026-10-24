# `omath::primitives::Mesh` — 3D mesh with transformation support

> Header: `omath/3d_primitives/mesh.hpp`
> Namespace: `omath::primitives`
> Depends on: `omath::Vector3<T>`, `omath::Mat<4,4,...>` (`Mat4X4` alias per engine), `omath::Triangle<Vector3<T>>`, `omath::primitives::Aabb`
> Purpose: represent and transform 3D meshes in different engine coordinate systems

---

## Overview

`Mesh` represents a 3D polygonal mesh with vertex data and transformation capabilities. It stores:
* **Vertex buffer (VBO)** — container of vertices (`Vertex<>` with position/normal/uv, or plain `Vector3` positions)
* **Element buffer (EBO)** — container of triangular faces (`Vector3<std::uint32_t>` indices into the VBO)
* **Transformation** — position, rotation, and scale with caching

The mesh supports transformation from local space to world space using engine-specific coordinate systems through the `MeshTrait` template parameter.

---

## Template Declaration

```cpp
template<class Mat4X4, class RotationAngles, class MeshTypeTrait,
         class VertType = Vertex<>,
         class VboType  = std::vector<VertType>,
         class EboType  = std::vector<Vector3<std::uint32_t>>>
class Mesh final;
```

### Template Parameters

* `Mat4X4` — Matrix type for transformations (the engine's `Mat4X4` alias)
* `RotationAngles` — Rotation representation (e.g., the engine's `ViewAngles` with pitch/yaw/roll)
* `MeshTypeTrait` — Engine-specific transformation trait (see [Engine Traits](#engine-traits))
* `VertType` — Vertex type used to derive `VectorType` (default `Vertex<>`)
* `VboType` — Vertex buffer container (default `std::vector<VertType>`); `std::array` is used by the box/plane builders
* `EboType` — Element buffer container (default `std::vector<Vector3<std::uint32_t>>`)

### `Vertex`

```cpp
template<class VecType = Vector3<float>, class UvT = Vector2<float>>
struct Vertex final {
    using VectorType = VecType;
    using UvType     = UvT;
    VectorType position;
    VectorType normal;
    UvType     uv;
};

template<typename T> concept HasPosition = requires(T vertex) { vertex.position; };
template<typename T> concept HasNormal   = requires(T vertex) { vertex.normal; };
template<typename T> concept HasUv       = requires(T vertex) { vertex.uv; };
```

The mesh treats a vertex that has a `position` member as a full vertex and any other element type as a bare position vector.

---

## Type Aliases

```cpp
using VectorType = VertType::VectorType;
using VertexType = VboType::value_type;
```

Pre-defined mesh aliases live in each engine namespace (`omath/engines/<engine>/mesh.hpp`):

```cpp
// e.g. Source, Frostbite, IW, OpenGL, Serious, CryEngine, RAGE
using Mesh = omath::primitives::Mesh<Mat4X4, ViewAngles, MeshTrait>;
```

Box and plane meshes use dedicated aliases (`BoxMesh`, `PlaneMesh`) from `omath/engines/<engine>/primitives.hpp`, which switch the buffers to `std::array`.

Use the pre-defined type aliases in engine namespaces:
```cpp
using namespace omath::source_engine;
Mesh my_mesh = /* ... */;  // Uses source_engine::Mesh
```

---

## Data Members

### Vertex Data

```cpp
Vbo m_vertex_buffer;           // VBO: vertices (positions in local space)
Ebo m_element_buffer_object;   // EBO: face indices
```

* `m_vertex_buffer` — vertices in **local space**
* `m_element_buffer_object` — triangular faces, each containing 3 indices into `m_vertex_buffer`

**Public access**: These members are public for direct manipulation when needed.

---

## Constructor

```cpp
Mesh(Vbo vbo, Ebo vao, const VectorType scale = {1, 1, 1});
```

Creates a mesh from vertex and index data.

**Parameters**:
* `vbo` — vertex buffer (moved into mesh)
* `vao` — element buffer (moved into mesh)
* `scale` — initial scale (default `{1, 1, 1}`)

**Example**:
```cpp
using omath::primitives::Vertex;

std::vector<Vertex<>> vertices = {
    {{0, 0, 0}, {}, {}}, {{1, 0, 0}, {}, {}}, {{0, 1, 0}, {}, {}}, {{0, 0, 1}, {}, {}}
};

std::vector<Vector3<std::uint32_t>> faces = {
    {0, 1, 2},  // Triangle 1
    {0, 1, 3},  // Triangle 2
    {0, 2, 3},  // Triangle 3
    {1, 2, 3}   // Triangle 4
};

using namespace omath::source_engine;
Mesh tetrahedron(std::move(vertices), std::move(faces));
```

---

## Transformation Methods

### Setting Transform Components

```cpp
void set_origin(const VectorType& new_origin);
void set_scale(const VectorType& new_scale);
void set_rotation(const RotationAngles& new_rotation_angles);
```

Update the mesh's transformation. **Side effect**: invalidates the cached transformation matrix, which will be recomputed on the next `get_to_world_matrix()` call.

**Example**:
```cpp
mesh.set_origin({10, 0, 5});
mesh.set_scale({2, 2, 2});

ViewAngles angles;
angles.pitch = PitchAngle::from_degrees(45.0f);
angles.yaw = YawAngle::from_degrees(30.0f);
mesh.set_rotation(angles);
```

### Getting Transform Components

```cpp
[[nodiscard]] const VectorType& get_origin() const;
[[nodiscard]] const VectorType& get_scale() const;
[[nodiscard]] const RotationAngles& get_rotation_angles() const;
```

Retrieve current transformation components.

### Transformation Matrix

```cpp
[[nodiscard]] const Mat4X4& get_to_world_matrix() const;
```

Returns the cached local-to-world transformation matrix. The matrix is computed lazily on first access after any transformation change:

```
M = Translation(origin) × Rotation(angles) × Scale(scale)
```

The rotation matrix is computed using the engine-specific `MeshTrait::rotation_matrix()` method.

**Caching**: The matrix is stored in a `mutable std::optional` and recomputed only when invalidated by `set_*` methods.

---

## Vertex Transformation

### `vertex_position_to_world_space`

```cpp
[[nodiscard]]
VectorType vertex_position_to_world_space(const Vector3<float>& vertex_position) const;
```

Transforms a vertex position from local space to world space by multiplying with the transformation matrix.

**Algorithm**:
1. Convert vertex to column matrix: `[x, y, z, 1]ᵀ`
2. Multiply by transformation matrix: `M × vertex`
3. Extract the resulting 3D position

**Usage**:
```cpp
Vector3<float> local_vertex{1, 0, 0};
auto world_vertex = mesh.vertex_position_to_world_space(local_vertex);
```

**Note**: This is used by `MeshCollider` to provide world-space support points for GJK/EPA.

---

## Face Transformation

### `make_face_in_local_space` / `make_face_in_world_space`

```cpp
[[nodiscard]]
Triangle<VectorType> make_face_in_local_space(const Ebo::const_iterator vao_iterator) const;

[[nodiscard]]
Triangle<VectorType> make_face_in_world_space(const Ebo::const_iterator vao_iterator) const;
```

Create a triangle from a face index iterator: the local-space variant uses the raw vertex positions, the world-space variant transforms all three through `vertex_position_to_world_space`. Indices are resolved with `m_vertex_buffer.at(...)`.

**Parameters**:
* `vao_iterator` — iterator to an element in `m_element_buffer_object`

**Example**:
```cpp
for (auto it = mesh.m_element_buffer_object.cbegin();
     it != mesh.m_element_buffer_object.cend();
     ++it) {
    Triangle<Vector3<float>> world_triangle = mesh.make_face_in_world_space(it);
    // Render or process the triangle
}
```

---

## Bounds

```cpp
[[nodiscard]]
Aabb<typename VectorType::ContainedType> local_bounds() const;
```

Axis-aligned bounds of the vertex buffer in the mesh's **own frame** (no transform applied). It is a plain min/max sweep over the vertices each time it is called, so hoist it out of query loops (for example as a broadphase reject in front of a face-by-face ray trace).

---

## Usage Examples

### Creating a Box Mesh

```cpp
using namespace omath::source_engine;

using omath::primitives::Vertex;
std::vector<Vertex<>> box_vbo = {
    // Bottom face
    {{-0.5f, -0.5f, 0.0f}, {}, {}}, {{ 0.5f, -0.5f, 0.0f}, {}, {}},
    {{ 0.5f,  0.5f, 0.0f}, {}, {}}, {{-0.5f,  0.5f, 0.0f}, {}, {}},
    // Top face
    {{-0.5f, -0.5f, 1.0f}, {}, {}}, {{ 0.5f, -0.5f, 1.0f}, {}, {}},
    {{ 0.5f,  0.5f, 1.0f}, {}, {}}, {{-0.5f,  0.5f, 1.0f}, {}, {}}
};

std::vector<Vector3<std::uint32_t>> box_vao = {
    // Bottom
    {0, 1, 2}, {0, 2, 3},
    // Top
    {4, 6, 5}, {4, 7, 6},
    // Sides
    {0, 4, 5}, {0, 5, 1},
    {1, 5, 6}, {1, 6, 2},
    {2, 6, 7}, {2, 7, 3},
    {3, 7, 4}, {3, 4, 0}
};

Mesh box(std::move(box_vbo), std::move(box_vao));
box.set_origin({0, 0, 50});
box.set_scale({10, 10, 10});
```

### Transforming Mesh Over Time

```cpp
void update_mesh(Mesh& mesh, float delta_time) {
    // Rotate mesh
    auto rotation = mesh.get_rotation_angles();
    rotation.yaw = YawAngle::from_degrees(
        rotation.yaw.as_degrees() + 45.0f * delta_time
    );
    mesh.set_rotation(rotation);
    
    // Oscillate position
    auto origin = mesh.get_origin();
    origin.z = 50.0f + 10.0f * std::sin(current_time * 2.0f);
    mesh.set_origin(origin);
}
```

### Collision Detection

```cpp
using namespace omath::collision;
using namespace omath::source_engine;

Mesh mesh_a(vbo_a, vao_a);
mesh_a.set_origin({0, 0, 0});

Mesh mesh_b(vbo_b, vao_b);
mesh_b.set_origin({5, 0, 0});

MeshCollider collider_a(std::move(mesh_a));   // omath::source_engine::MeshCollider
MeshCollider collider_b(std::move(mesh_b));

const bool hit = GjkAlgorithm<MeshCollider>::is_collide(
    collider_a, collider_b
);
```

### Rendering Transformed Triangles

```cpp
void render_mesh(const Mesh& mesh) {
    for (auto it = mesh.m_element_buffer_object.cbegin();
         it != mesh.m_element_buffer_object.cend();
         ++it) {
        
        Triangle<Vector3<float>> tri = mesh.make_face_in_world_space(it);
        
        // Draw triangle with your renderer
        draw_triangle(tri.m_vertex1, tri.m_vertex2, tri.m_vertex3);
    }
}
```

---

## Engine Traits

Each game engine has a corresponding `MeshTrait` that provides the `rotation_matrix` function:

```cpp
class MeshTrait final {
public:
    [[nodiscard]]
    static Mat4X4 rotation_matrix(const ViewAngles& rotation);
};
```

### Available Engines

| Engine | Namespace | Header |
|--------|-----------|--------|
| Source Engine | `omath::source_engine` | `engines/source_engine/mesh.hpp` |
| CryEngine | `omath::cry_engine` | `engines/cry_engine/mesh.hpp` |
| RAGE | `omath::rage_engine` | `engines/rage_engine/mesh.hpp` |
| Serious Engine | `omath::serious_engine` | `engines/serious_engine/mesh.hpp` |
| Unity | `omath::unity_engine` | `engines/unity_engine/mesh.hpp` |
| Unreal | `omath::unreal_engine` | `engines/unreal_engine/mesh.hpp` |
| Frostbite | `omath::frostbite_engine` | `engines/frostbite_engine/mesh.hpp` |
| IW Engine | `omath::iw_engine` | `engines/iw_engine/mesh.hpp` |
| OpenGL | `omath::opengl_engine` | `engines/opengl_engine/mesh.hpp` |

**Example** (Source Engine):
```cpp
using namespace omath::source_engine;

// Uses source_engine::MeshTrait automatically
Mesh my_mesh(vertices, indices);
```

See [MeshTrait Documentation](#mesh-trait-documentation) for engine-specific details.

---

## Performance Considerations

### Matrix Caching

The transformation matrix is computed lazily and cached:
* **First access**: O(matrix multiply) ≈ 64 float operations
* **Subsequent access**: O(1) — returns cached matrix
* **Cache invalidation**: Any `set_*` call invalidates the cache

**Best practice**: Batch transformation updates before accessing the matrix:
```cpp
// Good: single matrix recomputation
mesh.set_origin(new_origin);
mesh.set_rotation(new_rotation);
mesh.set_scale(new_scale);
auto matrix = mesh.get_to_world_matrix();  // Computes once

// Bad: three matrix recomputations
mesh.set_origin(new_origin);
auto m1 = mesh.get_to_world_matrix();  // Compute
mesh.set_rotation(new_rotation);
auto m2 = mesh.get_to_world_matrix();  // Compute again
mesh.set_scale(new_scale);
auto m3 = mesh.get_to_world_matrix();  // Compute again
```

### Memory Layout

* **VBO**: Contiguous `std::vector` for cache-friendly access
* **VAO**: Contiguous indices for cache-friendly face iteration
* **Matrix**: Cached in `std::optional` (no allocation)

### Transformation Cost

* `vertex_position_to_world_space`: ~15-20 FLOPs per vertex (4×4 matrix multiply)
* `make_face_in_world_space`: ~60 FLOPs (3 vertices)

For high-frequency transformations, consider:
* Caching transformed vertices if the mesh doesn't change
* Using simpler proxy geometry for collision
* Batching transformations

---

## Coordinate System Details

Different engines use different coordinate systems:

| Engine | Up Axis | Forward Axis | Handedness |
|--------|---------|--------------|------------|
| Source | +Z | +Y | Right |
| Unity | +Y | +Z | Left |
| Unreal | +Z | +X | Left |
| Frostbite | +Y | +Z | Right |
| IW Engine | +Z | +Y | Right |
| OpenGL | +Y | +Z | Right |

The `MeshTrait::rotation_matrix` function accounts for these differences, ensuring correct transformations in each engine's space.

---

## Limitations & Edge Cases

### Empty Mesh

A mesh with no vertices or faces is valid but not useful:
```cpp
Mesh empty_mesh({}, {});  // Valid but meaningless
```

For collision detection, ensure `m_vertex_buffer` is non-empty.

### Index Validity

Face indices are resolved with `m_vertex_buffer.at(...)` (throws `std::out_of_range` on a bad index). Ensure all indices are valid:
```cpp
assert(face.x < mesh.m_vertex_buffer.size());
assert(face.y < mesh.m_vertex_buffer.size());
assert(face.z < mesh.m_vertex_buffer.size());
```

### Degenerate Triangles

Faces with duplicate indices or collinear vertices will produce degenerate triangles. The mesh doesn't validate this; users must ensure clean geometry.

### Thread Safety

* **Read-only**: Safe to read from multiple threads (including const methods)
* **Modification**: Not thread-safe; synchronize `set_*` calls externally
* **Matrix cache**: Uses `mutable` member; not thread-safe even for const methods

---

## See Also

- [MeshCollider Documentation](../collision/mesh_collider.md) - Collision wrapper for meshes
- [GJK Algorithm Documentation](../collision/gjk_algorithm.md) - Uses mesh for collision detection
- [EPA Algorithm Documentation](../collision/epa_algorithm.md) - Penetration depth with meshes
- [Triangle Documentation](../linear_algebra/triangle.md) - Triangle primitive
- [Mat4X4 Documentation](../linear_algebra/mat.md) - Transformation matrices
- [Box Documentation](box.md) - Box primitive
- [Plane Documentation](plane.md) - Plane primitive

---

## Mesh Trait Documentation

For engine-specific `MeshTrait` details, see:

- [Source Engine MeshTrait](../engines/source_engine/mesh_trait.md)
- [Unity Engine MeshTrait](../engines/unity_engine/mesh_trait.md)
- [Unreal Engine MeshTrait](../engines/unreal_engine/mesh_trait.md)
- [CryEngine MeshTrait](../engines/cry_engine/mesh_trait.md)
- [RAGE Engine MeshTrait](../engines/rage_engine/mesh_trait.md)
- [Serious Engine MeshTrait](../engines/serious_engine/mesh_trait.md)
- [Frostbite Engine MeshTrait](../engines/frostbite/mesh_trait.md)
- [IW Engine MeshTrait](../engines/iw_engine/mesh_trait.md)
- [OpenGL Engine MeshTrait](../engines/opengl_engine/mesh_trait.md)

---

*Last updated: 13 Nov 2025*
