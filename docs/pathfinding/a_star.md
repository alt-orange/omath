# `omath::pathfinding::Astar` — Pathfinding over a navigation mesh

> Header: `omath/pathfinding/a_star.hpp`
> Namespace: `omath::pathfinding`
> Inputs: start/end as `Vector3<float>`, a `NavigationMesh`
> Output: ordered list of waypoints `std::vector<Vector3<float>>`

`Astar` exposes a single public function, `find_path`, that computes a path of 3D waypoints on a navigation mesh. Internally it reconstructs the result with the private `reconstruct_final_path` from a closed set keyed by `Vector3<float>`. The implementation lives in `source/pathfinding/a_star.cpp`.

---

## API

```cpp
namespace omath::pathfinding {

struct PathNode; // holds per-node search data (see "Expected PathNode fields")

class Astar final {
public:
  [[nodiscard]]
  static std::vector<Vector3<float>>
  find_path(const Vector3<float>& start,
            const Vector3<float>& end,
            const NavigationMesh& nav_mesh) noexcept;

private:
  [[nodiscard]]
  static std::vector<Vector3<float>>
  reconstruct_final_path(
    const std::unordered_map<Vector3<float>, PathNode>& closed_list,
    const Vector3<float>& current) noexcept;
};

} // namespace omath::pathfinding
```

### Semantics

* Returns a **polyline** of 3D points from the mesh vertex closest to `start` to the mesh vertex closest to `end` (both are snapped with `NavigationMesh::get_closest_vertex`; the raw `start`/`end` are not included).
* Returns an **empty vector** if either snap fails (empty mesh) or no path exists.
* If both points snap to the same vertex, the result is that single vertex.

---

## What `find_path` uses from `NavigationMesh`

* `get_closest_vertex(point)` — snaps `start` and `end` to graph vertices.
* `get_neighbors(vertex)` — adjacency list of the current vertex.
* Edge cost `g(u,v)` is the Euclidean distance `v.distance_to(u)` between the vertices; the heuristic is the Euclidean distance to the end vertex.

See [`NavigationMesh`](navigation_mesh.md).

---

## `PathNode`

`PathNode` is declared in the header but defined in `a_star.cpp`:

```cpp
struct PathNode final {
  std::optional<Vector3<float>> came_from;  // predecessor vertex (empty for the start)
  float g_cost = 0.f;                       // cost from the start vertex
};
```

`reconstruct_final_path(closed_list, current)` walks `came_from` links from `current` back to the start, **reverses** the chain, and returns the path.

---

## Heuristic & optimality

* Use an **admissible** heuristic (never overestimates true cost) to keep A* optimal.
  The usual choice is **Euclidean distance** in 3D:

  ```cpp
  h(x, goal) = (goal - x).length();
  ```
* For best performance, make it **consistent** (triangle inequality holds). Euclidean distance is consistent on standard navmeshes.

---

## Complexity

Let `V` be explored vertices (or portal nodes) and `E` the traversed edges.

* With a binary heap open list: **O(E log V)** time, **O(V)** memory.
* With a d-ary heap or pairing heap you may reduce practical constants.

---

## Typical usage

```cpp
#include "omath/pathfinding/a_star.hpp"
#include "omath/pathfinding/navigation_mesh.hpp"

using omath::Vector3;
using omath::pathfinding::Astar;

omath::pathfinding::NavigationMesh nav = /* ... load/build mesh ... */;

Vector3<float> start{2.5f, 0.0f, -1.0f};
Vector3<float> goal {40.0f, 0.0f, 12.0f};

auto path = Astar::find_path(start, goal, nav);

if (!path.empty()) {
  // feed to your agent/renderer
  for (const auto& p : path) {
    // draw waypoint p or push to steering
  }
} else {
  // handle "no path" (e.g., unreachable or disconnected mesh)
}
```

---

## Notes & recommendations

* **Start/end snapping**: `start` and `end` are always snapped to the nearest graph vertex (linear scan); the returned path therefore begins and ends on graph vertices, not at the exact input points.
* **Numerical stability**: Prefer squared distances when only comparing (`dist2`) to avoid unnecessary `sqrt`.
* **Tie-breaking**: When `f` ties are common (grid-like graphs), bias toward larger `g` or smaller `h` to reduce zig-zagging.
* **Smoothing**: A* returns a polyline that may hug polygon edges. Consider:

    * **String pulling / Funnel algorithm** over the corridor of polygons to get a straightened path.
    * **Raycast smoothing** (visibility checks) to remove redundant interior points.
* **Hashing `Vector3<float>`**: The library defines `std::hash<omath::Vector3<float>>`. Ensure equality/precision rules for using float keys are acceptable (or use discrete node IDs instead).
* **Missing vertices**: `get_neighbors` is `noexcept` but uses `m_vertex_map.at()`, so every neighbor listed in the graph must also exist as a key of `m_vertex_map`.

---

## Testing checklist

* Start/end snapping to the **same vertex** → path of 1 point.
* **Disconnected components** → empty result.
* **Narrow corridors** → path stays inside.
* **Obstacles blocking** → no path.
* **Floating-point noise** → still reconstructs a valid chain from parents.

---

## Implementation outline (for reference)

```cpp
// Simplified from source/pathfinding/a_star.cpp
find_path(start, end, mesh):
  start_v = mesh.get_closest_vertex(start); end_v = mesh.get_closest_vertex(end)
  if either failed: return {}

  open  = min-heap by f_cost            // f = g + distance(neighbor, end_v)
  nodes = { start_v -> {came_from = none, g = 0} }
  closed = {}

  while open not empty:
    current = open.pop()
    if current in closed: continue
    closed[current] = nodes[current]
    if current == end_v: return reconstruct_final_path(closed, current)

    for neighbor in mesh.get_neighbors(current):
      if neighbor in closed: continue
      g = nodes[current].g + distance(neighbor, current)
      if neighbor not in nodes or g < nodes[neighbor].g:
        nodes[neighbor] = {came_from = current, g}
        open.push(neighbor, g + distance(neighbor, end_v))
  return {}   // no path
```

---

## FAQ

* **Why return `std::vector<Vector3<float>>` and not polygon IDs?**
  Waypoints are directly usable by agents/steering and for rendering. If you also need the corridor (polygon chain), extend the API or `PathNode` to store it.

* **Does `find_path` modify the mesh?**
  No; it takes the mesh by `const&` and only reads it.

---

*Last updated: 31 Oct 2025*
