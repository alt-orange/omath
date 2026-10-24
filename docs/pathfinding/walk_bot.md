# `omath::pathfinding::WalkBot` — per-tick waypoint follower on a navigation mesh

> Header: `omath/pathfinding/walk_bot.hpp` (implementation in `source/pathfinding/walk_bot.cpp`)
> Namespace: `omath::pathfinding`
> Depends on: [`NavigationMesh`](navigation_mesh.md), [`Astar`](a_star.md), [`Vector3`](../linear_algebra/vector3.md)
> Purpose: given the bot's position every tick, report the next waypoint to walk to and a coarse status (`IDLE`, `PATHING`, `FINISHED`)

---

## API

```cpp
namespace omath::pathfinding {

enum class WalkBotStatus { IDLE, PATHING, FINISHED };

class WalkBot {
public:
  WalkBot() = default;
  explicit WalkBot(const std::shared_ptr<NavigationMesh>& mesh, float min_node_distance = 1.f);

  void set_nav_mesh(const std::shared_ptr<NavigationMesh>& mesh);
  void set_min_node_distance(float distance);

  void set_target(const Vector3<float>& target);

  // Forget which node was last reached, so the bot can be re-routed from scratch.
  void reset();

  // Call every game tick with the bot's current world position.
  void update(const Vector3<float>& bot_position);

  void on_path(const std::function<void(const Vector3<float>&)>& callback);
  void on_status(const std::function<void(WalkBotStatus)>& callback);
};

} // namespace omath::pathfinding
```

`WalkBot` does not move anything. It computes where to go; your code reads the waypoint from the `on_path` callback and moves the character (set view angles, press keys, teleport, ...).

---

## Behavior of `update(bot_position)`

Each call runs these steps in order and stops at the first that applies:

| # | Condition | Effect |
|---|---|---|
| 1 | No target set | Nothing happens, no callbacks fire. |
| 2 | Distance to the target <= `min_node_distance` | `on_status(FINISHED)`, then return. |
| 3 | No `on_path` callback registered | Return. No status is reported. |
| 4 | Navigation mesh is unset or has been destroyed | `on_status(IDLE)`, then return. |
| 5 | `Astar::find_path(bot_position, target, mesh)` returns an empty path | `on_status(IDLE)`, then return. |
| 6 | Otherwise | `on_path(next_node)` then `on_status(PATHING)`. |

### Choosing `next_node`

The path is recomputed from the bot's current position on every call. Let `nearest = path.front()`:

1. If the bot is within `min_node_distance` of `nearest`, `nearest` is remembered as the last visited node.
2. If `nearest` equals the last visited node and the path has more than one node, the bot is sent to `path[1]`. This prevents it from turning back to a node it has just reached.
3. Otherwise it is sent to `nearest`.

If the bot is pushed away (knockback, respawn, teleport) to a node it has not visited, `nearest` differs from the last visited node, so the bot is routed to that node first and then continues forward.

### Ownership and lifetime

* The bot stores only a `std::weak_ptr<NavigationMesh>`. Keep the `shared_ptr` alive for as long as the bot is used; once it expires, `update` reports `IDLE`.
* `on_path` and `on_status` each hold a single callback; registering again replaces the previous one.
* `reset()` clears only the visited-node memory. The target, mesh, callbacks and `min_node_distance` are kept.
* `min_node_distance` is used both as the "reached the target" radius and as the "reached the node" radius. The default is `1.0`.

---

## Usage

```cpp
#include <omath/pathfinding/walk_bot.hpp>

using namespace omath;
using namespace omath::pathfinding;

auto mesh = std::make_shared<NavigationMesh>();
mesh->deserialize(saved_mesh_text);   // or fill mesh->m_vertex_map directly

WalkBot bot(mesh, /*min_node_distance=*/24.f);
bot.set_target({1200.f, -340.f, 64.f});

bot.on_path([](const Vector3<float>& next)
{
    walk_towards(next);          // your movement code
});

bot.on_status([&](WalkBotStatus status)
{
    switch (status)
    {
        case WalkBotStatus::PATHING:  break;
        case WalkBotStatus::IDLE:     /* no mesh or no route to the target */ break;
        case WalkBotStatus::FINISHED: stop_walking(); break;
    }
});

// Game loop
void tick()
{
    bot.update(get_local_player_position());
}

// New destination
bot.reset();
bot.set_target(new_destination);
```

---

## Notes

* `FINISHED` is reported on every tick while the bot stays within `min_node_distance` of the target; it is not a one-shot event. Stop calling `update` or ignore it after the first one.
* The A* search runs on every tick. Keep the mesh small enough for that, or call `update` at a lower rate than the game tick.
* `set_target` does not call `reset()`. Call `reset()` yourself when you switch to a destination in a different part of the mesh.
* The status callback is optional. The path callback is required for anything other than `FINISHED` to happen.

---

## See also

* [`Astar`](a_star.md)
* [`NavigationMesh`](navigation_mesh.md)
