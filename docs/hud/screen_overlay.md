# `omath::hud::ScreenOverlay` — crosshair, FOV circle, corner text and other screen-space widgets

> Header: `omath/hud/screen_overlay.hpp`
> Namespaces: `omath::hud` (class), `omath::hud::widget` (descriptors)
> Purpose: draw HUD elements positioned relative to the screen centre or corners instead of an entity box

---

## API

```cpp
namespace omath::hud {

class ScreenOverlay final {
public:
  ScreenOverlay(const Vector2<float>& screen_size,
                const std::shared_ptr<HudRendererInterface>& renderer);

  ScreenOverlay& add_crosshair(const widget::Crosshair&);
  ScreenOverlay& add_fov_circle(const widget::FovCircle&);
  ScreenOverlay& add_threat_arrow(const widget::ThreatArrow&);
  ScreenOverlay& add_hit_marker(const widget::HitMarker&);
  ScreenOverlay& add_center_bar(const widget::CenterBar&);
  ScreenOverlay& add_dashed_center_bar(const widget::DashedCenterBar&);
  ScreenOverlay& add_corner(const widget::Corner&);

  // Declarative interface: widget structs and std::optional<W> of them, drawn in order.
  template<typename... Widgets>
  ScreenOverlay& contents(Widgets&&... widgets);
};

}
```

`screen_size` is the size of the drawing surface in pixels; the overlay's centre is `screen_size / 2`. Construct a `ScreenOverlay` once per frame (it is a light value object) and call `contents()` or the `add_*` methods. Like the entity overlay, it only issues calls on the `HudRendererInterface` and never null-checks it.

---

## Widgets

All descriptors live in `omath::hud::widget`.

### `Crosshair`

```cpp
struct Crosshair {
  Color color;
  float gap = 4.f;            // empty space between the centre and each arm
  float length = 8.f;         // length of one arm
  float thickness = 1.f;
  Color outline{0,0,0,0};     // alpha 0 = no outline
  float dot_radius = 0.f;     // > 0 draws a centre dot
};
```

The four arms are drawn as filled rectangles (not lines) so they stay pixel-aligned with the centre dot on backends that nudge lines by half a pixel. A visible `outline` expands each arm and the dot by one pixel.

### `FovCircle`

```cpp
struct FovCircle {
  Color color;                // ring colour
  float radius;
  Paint fill{Color{0,0,0,0}}; // solid colour or Gradient (optionally animated)
  float thickness = 1.f;
  int   segments = 0;         // 0 = backend default
};
```

The fill is drawn first, then the ring on top. A transparent solid `fill` is skipped; a `Gradient` is always drawn.

### `ThreatArrow`

```cpp
struct ThreatArrow {
  float angle;                // radians; direction is (cos(angle), sin(angle)) in screen space
  Color color;
  float radius = 100.f;       // distance from the centre to the arrow tip
  float size = 12.f;
  Color outline{0,0,0,0};
};
```

A filled triangle pointing outward from the centre, used to indicate an off-screen threat. Angle `0` points right (+X); because screen `y` grows downward, increasing angles rotate clockwise on screen. Compute the angle yourself from the target's direction, for example with `atan2` on a 2D offset such as the one produced by [Radar](../algorithm/radar.md).

### `HitMarker`

```cpp
struct HitMarker {
  Color color;
  float size = 8.f;           // length of each tick
  float gap = 3.f;            // distance from the centre to the inner end of each tick
  float thickness = 2.f;
  Color outline{0,0,0,0};
  float alpha = 1.f;          // fade multiplier, clamped to [0, 1]
};
```

Four diagonal ticks around the centre. The widget has no clock: drive `alpha` yourself (for example `1.f - elapsed / fade_duration`) and stop submitting it when it reaches zero. `alpha` multiplies both the colour and the outline alpha.

### `CenterBar` and `DashedCenterBar`

```cpp
struct CenterBar {
  enum class Side { LEFT, RIGHT };

  Side side;
  BarPaint color;
  Color outline{0,0,0,0};
  Color bg{0,0,0,0};
  float length;                       // vertical extent, centred on the screen's vertical middle
  float thickness = 6.f;              // horizontal extent
  float ratio;                        // [0, 1], fills bottom-to-top
  float offset = 20.f;                // distance from the screen centre to the bar's inner edge
  std::optional<CanvasGlow> glow = std::nullopt;
};

struct DashedCenterBar {
  CenterBar::Side side;
  BarPaint color;
  Color outline{0,0,0,0};             // also the colour of the gaps between dashes
  Color bg{0,0,0,0};
  float length;
  float thickness = 6.f;
  float ratio;
  float dash_len = 8.f;
  float gap_len = 5.f;
  float offset = 20.f;
};
```

Use one `LEFT` and one `RIGHT` instance to flank a crosshair (or anything else at the centre). `CenterBar` supports a `CanvasGlow` that wraps only the filled part, so it grows with the bar; `DashedCenterBar` has no glow. Both accept solid, `Gradient` and `BarGradient` fills.

### `Corner`

```cpp
enum class Anchor { TOP_LEFT, TOP_CENTER, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT };

using CornerWidget = std::variant<None, Label, SpaceVertical>;

struct Corner {
  Anchor anchor;
  std::initializer_list<CornerWidget> children;
  float padding = 10.f;

  Corner(Anchor a, std::initializer_list<CornerWidget> c, float p = 10.f);
};
```

A vertical stack of labels anchored to a screen edge or corner. Children are laid out in order and grow away from the anchored edge: downward from `TOP_*`, upward from `BOTTOM_*`. `padding` is the distance from the edge(s). `LEFT`/`RIGHT` anchors align the text to that edge and `CENTER` centres each item's own width. `Label::offset` moves that one label away from the anchored edge without advancing the stack, and `SpaceVertical` adds a gap in the growth direction. See [Widgets](widgets.md) for `Label`.

---

## Usage

```cpp
#include <omath/hud/renderer_realizations/imgui_renderer.hpp>
#include <omath/hud/screen_overlay.hpp>

void draw_hud(float screen_w, float screen_h, float hp_ratio, float seconds_since_hit)
{
    using namespace omath::hud::widget;
    using omath::Color;
    using omath::hud::when;

    static const auto renderer = std::make_shared<omath::hud::ImguiHudRenderer>();

    constexpr float hit_fade = 0.4f;
    const Color black{0.f, 0.f, 0.f, 1.f};
    const BarPaint hp_paint{BarGradient{Color::red(), Color::green()}};

    omath::hud::ScreenOverlay({screen_w, screen_h}, renderer)
        .contents(
            Crosshair{Color::green(), 4.f, 8.f, 1.f, black, 1.5f},
            CenterBar{CenterBar::Side::LEFT, hp_paint, black, Color{0.1f, 0.1f, 0.1f, 0.8f},
                      80.f, 6.f, hp_ratio, 24.f},
            when(seconds_since_hit < hit_fade,
                 HitMarker{Color{1.f, 1.f, 1.f, 1.f}, 8.f, 3.f, 2.f, black, 1.f - seconds_since_hit / hit_fade}),
            Corner{Anchor::TOP_LEFT, {Label{Color::green(), 0.f, Outlined::On, "omath::hud"}}},
            Corner{Anchor::BOTTOM_RIGHT,
                   {Label{Color{0.8f, 0.8f, 0.8f, 1.f}, 0.f, Outlined::On, "Ping: 24ms"}}});
}
```

`CenterBar` is an aggregate whose members must be given in declaration order, so `thickness` has to be supplied before `ratio`.

---

## Notes

* `ScreenOverlay` does not clip to the screen; positions are computed from `screen_size` alone. Pass the real size of the surface you are drawing on, and update it when the window is resized.
* `Corner` holds a `std::initializer_list`, so do not store a `Corner` in a variable from a braced list; build it inside the `contents(...)` or `add_corner(...)` call.
* `Corner` children cannot be wrapped in `widget::when`; wrap the whole `Corner` in `omath::hud::when` instead.
* `FovCircle` draws a screen-space circle. It does not know about the game's field of view; compute the pixel radius from your aim FOV and camera yourself.

---

## See also

* [HUD overview](overview.md)
* [EntityOverlay](entity_overlay.md)
* [Widgets](widgets.md)
* [HudRendererInterface](renderer_interface.md)
* [Radar](../algorithm/radar.md)
