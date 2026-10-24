# `omath::algorithm::get_closest_target_by_*` — pick the best target from a range

> Header: `omath/algorithm/targeting.hpp`
> Namespace: `omath::algorithm`
> Depends on: [`Vector3`](../linear_algebra/vector3.md), a [`Camera`](../projection/camera.md) (FOV variant only)
> Purpose: select one element from a sequence by smallest angle to the crosshair, or by smallest world-space distance

---

## API

```cpp
namespace omath::algorithm {

// By angular distance to the camera's view direction
template<class CameraType, std::input_or_output_iterator IteratorType, class FilterT>
[[nodiscard]]
IteratorType get_closest_target_by_fov(const IteratorType& begin, const IteratorType& end,
                                       const CameraType& camera, auto get_position,
                                       const std::optional<std::function<FilterT>>& filter_func = std::nullopt);

template<class CameraType, std::ranges::range RangeType, class FilterT>
[[nodiscard]]
auto get_closest_target_by_fov(const RangeType& range, const CameraType& camera, auto get_position,
                               const std::optional<std::function<FilterT>>& filter_func = std::nullopt);

// By world-space distance to an origin
template<std::input_or_output_iterator IteratorType, class FilterT>
[[nodiscard]]
IteratorType get_closest_target_by_distance(const IteratorType& begin, const IteratorType& end,
                                            const Vector3<float>& origin, auto get_position,
                                            const std::optional<std::function<FilterT>>& filter_func = std::nullopt);

template<std::ranges::range RangeType, class FilterT>
[[nodiscard]]
auto get_closest_target_by_distance(const RangeType& range, const Vector3<float>& origin, auto get_position,
                                    const std::optional<std::function<FilterT>>& filter_func = std::nullopt);

} // namespace omath::algorithm
```

Each function has an iterator-pair overload and a range overload that forwards to it. Constraints omitted above: `FilterT` must be a function signature such that `std::function<FilterT>` is invocable with the element and returns `bool`.

---

## Parameters

| Parameter | Meaning |
|---|---|
| `begin`, `end` / `range` | The candidates. Any input iterator pair or range. |
| `camera` | (FOV variant) A camera providing `get_view_angles()` and `calc_look_at_angles(position)`. |
| `origin` | (distance variant) The point distances are measured from. |
| `get_position` | Callable mapping an element to its `Vector3<float>` world position. |
| `filter_func` | Optional predicate on the element; elements for which it returns `false` are skipped. |

---

## Behavior

* **Return value** is an iterator to the chosen element, or `end` if the range is empty or every element was filtered out. Always compare with `end` before dereferencing.
* **Ties** go to the first element: a later element only wins when it is strictly closer.
* **`get_closest_target_by_distance`** compares `origin.distance_to(get_position(e))`, a full 3D distance.
* **`get_closest_target_by_fov`** compares view angles in degrees: the angles from `camera.calc_look_at_angles(position)` against the camera's current `get_view_angles()`. The element with the smallest angular difference to where the camera is looking wins, which is the "closest to crosshair" target for an aimbot or a target selector.
* **`FilterT` cannot be deduced** from `std::nullopt`, so you always have to spell the template arguments (see below).

---

## Usage

```cpp
#include <omath/algorithm/targeting.hpp>
#include <omath/engines/source_engine/camera.hpp>
#include <vector>

using Camera = omath::source_engine::Camera;
using Targets = std::vector<omath::Vector3<float>>;
using Iter = Targets::const_iterator;
using FilterSig = bool(const omath::Vector3<float>&);

auto get_pos = [](const omath::Vector3<float>& v) -> const omath::Vector3<float>& { return v; };

Targets targets = {{100.f, 50.f, 0.f}, {100.f, 1.f, 0.f}, {100.f, -30.f, 0.f}};

// Closest to the crosshair
const auto aimed = omath::algorithm::get_closest_target_by_fov<Camera, Iter, FilterSig>(
    targets.cbegin(), targets.cend(), camera, get_pos);

// Nearest in the world
const auto nearest = omath::algorithm::get_closest_target_by_distance<Iter, FilterSig>(
    targets.cbegin(), targets.cend(), camera.get_origin(), get_pos);

if (nearest != targets.cend())
    use(*nearest);
```

With a filter and the range overload:

```cpp
struct Player { omath::Vector3<float> pos; bool alive; };
std::vector<Player> players = /* ... */;

using PlayerFilter = bool(const Player&);
const std::optional<std::function<PlayerFilter>> only_alive = [](const Player& p) { return p.alive; };

const auto best = omath::algorithm::get_closest_target_by_distance<std::vector<Player>, PlayerFilter>(
    players, camera.get_origin(), [](const Player& p) { return p.pos; }, only_alive);
```

---

## Notes

!!! warning "Angle distance details"
    The FOV variant measures the current candidate as a 2D distance between `(pitch, yaw)` pairs, but measures the best-so-far candidate as a 3D distance that also includes `roll`. With a camera whose roll is zero (the usual case) both are the same. Angles are compared as plain degrees with no wrap-around at 360, so targets near the yaw wrap point can be mis-ranked.

* The filter path is not covered by the unit tests in `tests/general/unit_test_targeting.cpp`; the examples above follow the declared signatures.
* The range overloads take the range by `const&`, so the returned iterator is a `const_iterator` of the range.

---

## See also

* [Radar projection](radar.md)
* [Camera](../projection/camera.md)
* [View angles](../trigonometry/view_angles.md)
