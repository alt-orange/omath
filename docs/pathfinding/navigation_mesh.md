# `omath::pathfinding::NavigationMesh` — Lightweight vertex graph for A*

> Header: `omath/pathfinding/navigation_mesh.hpp`
> Namespace: `omath::pathfinding`
> Nodes: `Vector3<float>` (3D points)
> Storage: adjacency map `unordered_map<Vector3<float>, std::vector<Vector3<float>>>`

A minimal navigation mesh represented as a **vertex/edge graph**. Each vertex is a `Vector3<float>` and neighbors are stored in an adjacency list. Designed to pair with `Astar::find_path`.

---

## API

```cpp
class NavigationMesh final {
public:
  // Nearest graph vertex to an arbitrary 3D point.
  // On success -> closest vertex; on failure -> std::string error (e.g., empty mesh).
  [[nodiscard]]
  std::expected<Vector3<float>, std::string>
  get_closest_vertex(const Vector3<float>& point) const noexcept;

  // Read-only neighbor list for a vertex key.
  // Implemented with m_vertex_map.at(): an absent vertex throws std::out_of_range
  // (and since the function is noexcept, that terminates the program).
  [[nodiscard]]
  const std::vector<Vector3<float>>&
  get_neighbors(const Vector3<float>& vertex) const noexcept;

  // True if the graph has no vertices.
  [[nodiscard]]
  bool empty() const;

  // Events -- per-vertex optional tag (e.g. "jump", "teleport")
  void set_event(const Vector3<float>& vertex, const std::string_view& event_id); // throws std::invalid_argument if vertex is unknown
  void clear_event(const Vector3<float>& vertex);
  [[nodiscard]]
  std::optional<std::string> get_event(const Vector3<float>& vertex) const noexcept;

  // Serialize/deserialize the graph (whitespace-separated text).
  [[nodiscard]] std::string serialize() const noexcept;
  void deserialize(const std::string& raw);   // throws std::runtime_error on truncated data

  // Public adjacency (vertex -> neighbors)
  std::unordered_map<Vector3<float>, std::vector<Vector3<float>>> m_vertex_map;

private:
  std::unordered_map<Vector3<float>, std::string> m_vertex_events;
};
```

---

## Quick start

```cpp
using omath::Vector3;
using omath::pathfinding::NavigationMesh;

// Build a tiny mesh (triangle)
NavigationMesh nav;
nav.m_vertex_map[ {0,0,0} ] = { {1,0,0}, {0,0,1} };
nav.m_vertex_map[ {1,0,0} ] = { {0,0,0}, {0,0,1} };
nav.m_vertex_map[ {0,0,1} ] = { {0,0,0}, {1,0,0} };

// Query the closest node to an arbitrary point
auto q = nav.get_closest_vertex({0.3f, 0.0f, 0.2f});
if (q) {
  const auto& v = *q;
  const auto& nbrs = nav.get_neighbors(v);
  (void)nbrs;
}
```

---

## Semantics & expectations

* **Nearest vertex**
  `get_closest_vertex(p)` scans the keys of `m_vertex_map` and returns the one with minimal Euclidean distance to `p`. If the mesh is empty, it returns `std::unexpected("Failed to get closest point")`.

* **Neighbors**
  `get_neighbors(v)` returns the adjacency list for `v` via `m_vertex_map.at(v)`. If `v` is not a key of the map, `std::out_of_range` is thrown; because the function is declared `noexcept`, this ends in `std::terminate`. Only pass vertices that exist (e.g. results of `get_closest_vertex`).

* **Events**
  `set_event(v, id)` attaches an arbitrary string tag to an existing vertex (throws `std::invalid_argument` if `v` is not in the graph), `get_event(v)` returns it as `std::optional<std::string>`, and `clear_event(v)` removes it. Events are not used by `Astar`.

* **Graph invariants** (recommended)

    * Neighbor links are **symmetric** for undirected navigation (if `u` has `v`, then `v` has `u`).
    * No self-loops unless explicitly desired.
    * Vertices are unique keys; hashing uses `std::hash<Vector3<float>>` (be mindful of floating-point equality).

---

## Serialization

* `serialize()` → text of the current `m_vertex_map`, one block per vertex: a line `x y z neighbor_count event_id` (`event_id` is `-` when no event is set) followed by `neighbor_count` lines `nx ny nz`.
* `deserialize(raw)` → clears the graph and events, then rebuilds them from `raw`; throws `std::runtime_error` if the data ends in the middle of a neighbor list.
  Event ids are read as a single whitespace-delimited token, so they cannot contain spaces. There is no version header.

---

## Performance

Let `V = m_vertex_map.size()` and `E = Σ|neighbors(v)|`.

* `get_closest_vertex`: **O(V)** (linear scan) unless you back it with a spatial index (KD-tree, grid, etc.).
* `get_neighbors`: **O(1)** average (hash lookup).
* Memory: **O(V + E)**.

---

## Usage notes

* **Floating-point keys**: Using `Vector3<float>` as an unordered_map key relies on your `std::hash<omath::Vector3<float>>` and exact `operator==`. Avoid building meshes with numerically “close but not equal” duplicates; consider canonicalizing or using integer IDs if needed.
* **Pathfinding**: Pair with `Astar::find_path(start, end, nav)`; the A* heuristic can use straight-line distance between vertex positions.

---

## Minimal test ideas

* Empty mesh → `get_closest_vertex` returns error; `empty() == true`.
* Single vertex → nearest always that vertex; neighbors empty.
* Symmetric edges → `get_neighbors(a)` contains `b` and vice versa.
* Serialization round-trip preserves vertex/edge counts, neighbor lists and events.
