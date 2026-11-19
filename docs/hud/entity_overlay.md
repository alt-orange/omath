# `omath::hud::EntityOverlay` — ESP-style overlay around one entity

> Header: `omath/hud/entity_overlay.hpp`
> Namespace: `omath::hud`
> Purpose: draw boxes, bars, labels, rings, icons and skeletons around the 2D screen rectangle of a single entity, laying them out with per-side cursors

---

## API

```cpp
namespace omath::hud {

enum class EntityOverlayError {
  NO_PROJECTED_VERTEX,        // no vertex of the AABB could be projected
  CENTER_PROJECTION_FAILED,   // the AABB centre could not be projected
};

class EntityOverlay final {
public:
  EntityOverlay(const Vector2<float>& top, const Vector2<float>& bottom, float aspect,
                const std::shared_ptr<HudRendererInterface>& renderer);

  // Build from a world-space AABB and a camera.
  template<class Camera, class Aabb>
  [[nodiscard]]
  static std::expected<EntityOverlay, EntityOverlayError>
  from_aabb(const Camera& camera, const Aabb& aabb, float aspect,
            const std::shared_ptr<HudRendererInterface>& renderer);

  // Declarative interface: widget structs and std::optional<W> of them, drawn in order.
  template<typename... Widgets>
  EntityOverlay& contents(Widgets&&... widgets);

  // Imperative interface: every method returns *this for chaining (see below).
};

}
```

* `top` and `bottom` are screen-space points on the vertical centre line of the entity (head and feet); `bottom.x` is ignored and replaced by `top.x`. `aspect` is the height-to-half-width ratio (`CanvasBox`): the box is `2 * height / aspect` wide, so a larger value draws a narrower box. A value around `4` suits a standing humanoid.
* `renderer` is shared and not null-checked; every draw dereferences it. See [HudRendererInterface](renderer_interface.md).

---

## Creating an overlay

### From screen points

```cpp
omath::hud::EntityOverlay overlay({x, head_y}, {x, feet_y}, 4.f, renderer);
```

### From a world AABB

```cpp
auto overlay = omath::hud::EntityOverlay::from_aabb(camera, aabb, 4.f, renderer);
if (!overlay)
    return;   // overlay.error() is an EntityOverlayError
overlay->contents(/* ... */);
```

`from_aabb` projects all eight `aabb.vertices()` with `camera.world_to_screen_unclipped` and uses the exact screen-space bounding rectangle of the projected corners (min/max `x` and `y`) as the overlay box, the same mapping nullcore's ESP uses. If any corner fails to project (for example it is behind the camera) the entity is rejected, because the rectangle would be meaningless. The `aspect` argument is kept for source compatibility but is unused. The unclipped projection is used so that entities partly off-screen still get a full-size box.

| Error | When |
|-------|------|
| `NO_PROJECTED_VERTEX` | At least one of the eight vertices did not project, e.g. the box is (partly) behind the camera |
| `CENTER_PROJECTION_FAILED` | Reserved; no longer returned |

`Camera` and `Aabb` are templates: any camera that provides `world_to_screen_unclipped` (such as `omath::source_engine::Camera`, see [Camera](../projection/camera.md)) and any AABB with `vertices()` and `center()` works, including `double`-based types. The unit test `FromAabbSupportsDoubleCameraCoordinates` uses an Unreal camera with an `Aabb<double>`.

---

## Declarative interface

```cpp
using namespace omath::hud::widget;
using omath::hud::when;

overlay->contents(
    CanvasGlow{Glow{Color::red(), 24.f}},
    Box{Color::red(), Paint{Color{0.f, 0.f, 0.f, 0.2f}}, Color{0.f, 0.f, 0.f, 1.f}, 1.f},
    LeftSide{ Bar{BarPaint{Color::green()}, Color{0.f, 0.f, 0.f, 1.f}, Color{0.1f, 0.1f, 0.1f, 0.8f}, 4.f, 0.75f} },
    RightSide{ Label{Color::green(), 2.f, Outlined::On, "Health: 75"} },
    when(show_skeleton, Skeleton{Color{1.f, 1.f, 1.f, 1.f}}));
```

`contents()` accepts these top-level widgets (details on [Widgets](widgets.md)):

| Widget | Result |
|--------|--------|
| `Box` | Rectangle with optional fill (solid or gradient), outline and glow |
| `CorneredBox` | Four L-shaped corners, optional fill, outline and glow |
| `DashedBox` | Dashed rectangle |
| `CanvasGlow` | Soft glow around the whole canvas rectangle |
| `RightSide`, `LeftSide`, `TopSide`, `BottomSide` | Stacks of `SideWidget`s growing away from that edge |
| `Skeleton` | Default humanoid figure, or the bones you supply |
| `SnapLine` | Line from a screen point to the bottom-centre of the box |
| `ScanMarker` | Filled triangle near the head |
| `AimDot` | Filled circle at an absolute screen position |
| `ProjectileAim` | Marker at an absolute screen position, joined to the box centre by a line |

Any of them can be wrapped in `omath::hud::when(condition, widget)`.

---

## Layout model

The overlay keeps four cursors, one per side, starting at corners of the canvas box:

| Cursor | Starts at | Moves |
|--------|-----------|-------|
| right | top-right corner | `x` increases for bars and `SpaceHorizontal`; `y` increases for labels, rings, icons, `SpaceVertical` |
| left | top-left corner | `x` decreases for bars and `SpaceHorizontal`; `y` increases for labels, rings, icons, `SpaceVertical` |
| top | top-left corner | `y` decreases for every item (stacking upward); `SpaceHorizontal` moves `x` right |
| bottom | bottom-left corner | `y` increases for every item (stacking downward); `SpaceHorizontal` moves `x` right |

Left/right bars always span the full height of the box and grow from the bottom up; top/bottom bars span the full width and grow from left to right. Because bars advance the `x` cursor and labels advance the `y` cursor, a `RightSide` containing a bar followed by labels draws the labels to the right of the bar, starting at the top.

!!! note "Order matters"
    A cursor is only advanced by items drawn through it. Draw the bar before the labels that should sit next to it, and use `SpaceVertical`/`SpaceHorizontal` for extra gaps.

---

## Imperative interface

Each method draws immediately and returns `EntityOverlay&`. All `ratio` arguments are clamped to `[0, 1]`.

```cpp
// Boxes
add_2d_box(const Color& box_color, const Color& fill = {0,0,0,0}, const Color& outline = {0,0,0,0}, float thickness = 1.f);
add_2d_box(const Color& box_color, const Gradient& fill_gradient, const Color& outline = {0,0,0,0}, float thickness = 1.f);
add_cornered_2d_box(const Color& box_color, const Color& fill = {0,0,0,0}, const Color& outline = {0,0,0,0},
                    float corner_ratio_len = 0.2f, float thickness = 1.f);
add_dashed_box(const Color& color, float dash_len = 8.f, float gap_len = 5.f, float thickness = 1.f);

// Bars (also add_left_bar, add_top_bar, add_bottom_bar). `width` is `height` for top/bottom.
add_right_bar(const widget::BarPaint& color, const Color& outline, const Color& bg,
              float width, float ratio, float offset = 5.f,
              const std::optional<widget::Glow>& glow = std::nullopt);
// Dashed bars (also add_left_/add_top_/add_bottom_dashed_bar)
add_right_dashed_bar(const widget::BarPaint& color, const Color& outline, const Color& bg,
                     float width, float ratio, float dash_len, float gap_len, float offset = 5.f);

// Labels (also add_left_label, add_top_label, add_bottom_label, add_centered_top_label, add_centered_bottom_label)
add_right_label(const widget::Paint& color, float offset, widget::Outlined outlined,
                const std::string_view& text, const std::optional<widget::Glow>& glow = std::nullopt);
template<typename... Args> requires(sizeof...(Args) > 0)
add_right_label(const widget::Paint& color, float offset, widget::Outlined outlined,
                std::format_string<Args...> fmt, Args&&... args);

// Spacers: add_{right,left,top,bottom}_space_{vertical,horizontal}(float size)

// Progress rings (also add_left_/add_top_/add_bottom_progress_ring)
add_right_progress_ring(const Color& color, const Color& bg, float radius, float ratio,
                        float thickness = 2.f, float offset = 5.f, int segments = 0);

// Icons (also add_left_/add_top_/add_bottom_icon)
add_right_icon(const std::any& texture_id, float width, float height,
               const Color& tint = {1,1,1,1}, float offset = 5.f);

// Misc
add_snap_line(const Vector2<float>& start_pos, const Color& color, float width);
add_skeleton(const Color& color, float thickness = 1.f);                              // built-in humanoid
add_skeleton(std::span<const widget::Bone> bones, const Color& color, float thickness = 1.f);
```

Differences between the two interfaces:

* The `add_*_progress_ring` methods default to `segments = 0` (backend default), while `widget::ProgressRing` defaults to `segments = 32`.
* The `std::format` label overloads (`add_right_label(color, offset, outlined, "HP: {}", hp)`) only exist when at least one format argument is given, and only for the label methods; they format into a temporary and draw immediately.
* `add_top_*` and `add_bottom_*` items are positioned relative to the top-left and bottom-left corners; `add_centered_top_label` and `add_centered_bottom_label` centre the text on the box.
* There are no `add_*` methods for `CanvasGlow`, `ScanMarker`, `AimDot` or `ProjectileAim`; pass those through `contents()`.

---

## Example

```cpp
#include <omath/engines/source_engine/camera.hpp>
#include <omath/hud/entity_overlay.hpp>
#include <omath/hud/renderer_realizations/imgui_renderer.hpp>

void draw_player(const omath::source_engine::Camera& camera,
                 const omath::primitives::Aabb<float>& aabb,
                 float health, const std::shared_ptr<omath::hud::HudRendererInterface>& renderer)
{
    using namespace omath::hud::widget;
    using omath::Color;

    auto overlay = omath::hud::EntityOverlay::from_aabb(camera, aabb, 4.f, renderer);
    if (!overlay)
        return;

    const Color black{0.f, 0.f, 0.f, 1.f};

    overlay->contents(
        CorneredBox{Color::red(), Color{0.f, 0.f, 0.f, 0.15f}, black},
        LeftSide{Bar{BarPaint{BarGradient{Color::red(), Color::green()}},
                     black, Color{0.1f, 0.1f, 0.1f, 0.8f}, 4.f, health / 100.f}},
        TopSide{Centered{Label{Color{1.f, 1.f, 1.f, 1.f}, 2.f, Outlined::On, "Player"}}});
}
```

---

## Notes

* Overlays are cheap value objects: construct one per entity per frame, call `contents()`, and let it go out of scope.
* `Label::text` and the `initializer_list` held by side containers are non-owning; build them inside the `contents(...)` call.
* Text sizes come from `HudRendererInterface::calc_text_size`, which for the ImGui backend measures up to the terminating NUL. Pass NUL-terminated strings (literals, `std::string`) rather than slices of a larger buffer.
* `Centered<Label>` is only honoured by `TopSide`/`BottomSide`.
* The built-in skeleton is a stylised 15-bone figure proportioned to the box; for real bone data project your joints to screen space and use `Skeleton{bones, color}`.

---

## See also

* [HUD overview](overview.md)
* [Widgets](widgets.md)
* [ScreenOverlay](screen_overlay.md)
* [HudRendererInterface](renderer_interface.md)
* [AABB](../3d_primitives/aabb.md)
* [Camera](../projection/camera.md)
