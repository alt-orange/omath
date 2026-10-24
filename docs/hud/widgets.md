# `omath::hud::widget` — widget descriptors, paints and drawing helpers

> Headers: `omath/hud/entity_overlay_widgets.hpp`, `omath/hud/screen_overlay.hpp` (screen widgets), `omath/hud/widget_render.hpp`, `omath/hud/canvas_box.hpp`, `omath/hud/gradient.hpp`
> Namespaces: `omath::hud::widget` (widgets), `omath::hud` (everything else)
> Purpose: the vocabulary you hand to `EntityOverlay::contents()` / `ScreenOverlay::contents()`, plus the shared drawing routines behind it

---

## Where each widget can be used

| Widget | `EntityOverlay::contents` (top level) | Inside a side container | `ScreenOverlay::contents` |
|--------|:---:|:---:|:---:|
| `Box`, `CorneredBox`, `DashedBox`, `CanvasGlow` | yes | | |
| `Skeleton`, `SnapLine`, `ScanMarker`, `AimDot`, `ProjectileAim` | yes | | |
| `RightSide`, `LeftSide`, `TopSide`, `BottomSide` | yes | | |
| `Bar`, `DashedBar`, `Label`, `Centered<Label>`, `SpaceVertical`, `SpaceHorizontal`, `ProgressRing`, `Icon`, `None` | | yes (`SideWidget`) | |
| `Crosshair`, `FovCircle`, `ThreatArrow`, `HitMarker`, `CenterBar`, `DashedCenterBar`, `Corner` | | | yes |

`Corner` children are limited to `CornerWidget = std::variant<None, Label, SpaceVertical>`. Any top-level widget can also be passed as `std::optional<W>` (that is what `hud::when` returns); an empty optional draws nothing.

---

## Paints

```cpp
namespace omath::hud::widget {

struct Paint : std::variant<Color, Gradient> {              // solid or gradient
  using variant::variant;
  constexpr Paint(float r, float g, float b, float a);      // shorthand for Color{r,g,b,a}
};

struct BarGradient { Color empty_color; Color full_color; };

struct BarPaint : std::variant<Color, Gradient, BarGradient> {
  using variant::variant;
  constexpr BarPaint(float r, float g, float b, float a);
  BarPaint(const Paint& paint);                             // Paint -> BarPaint
};

}
```

* `Paint` is used for label text and for fills (`Box::fill`, `FovCircle::fill`). A `Color` selects a solid draw call, a `Gradient` selects the renderer's gradient call.
* `BarPaint` adds `BarGradient`, a fill that depends on the bar's `ratio`: the colour at the leading edge of the fill blends from `empty_color` (ratio 0) to `full_color` (ratio 1), and fades back to `empty_color` at the base of the bar (see `resolve_bar_paint`). The classic red-to-green health bar is `BarPaint{BarGradient{red, green}}`.
* Brace-initialising a paint from four floats works: `Label{{0.f, 1.f, 0.f, 1.f}, ...}` is a green label.

### `omath::hud::Gradient`

```cpp
enum class GradientDirection { RightToLeft, LeftToRight };

struct Gradient final {
  Color top_left;
  Color top_right;
  Color bottom_right;
  Color bottom_left;
  bool  animated = false;
  float animation_speed = 4.f;
  float animation_spread = 0.7f;
  GradientDirection direction = GradientDirection::RightToLeft;
};
```

The four corner colours are interpolated bilinearly over the filled rectangle, text bounding box, or circle bounding box. When `animated` is set, a renderer that supports it cycles between `top_left` and `top_right` over time: `animation_speed` scales the clock and `animation_spread` controls how quickly the phase changes from one glyph (or position around a circle) to the next. `direction` flips the travel direction. The ImGui backend animates text and filled circles; see [ImguiHudRenderer](imgui_renderer.md#gradients).

```cpp
const omath::hud::Gradient banner{
    Color::red(), Color::green(), Color::green(), Color::red(),
    /*animated*/ true, /*speed*/ 4.f, /*spread*/ 0.7f};
// used as a paint:
Label{Paint{banner}, 2.f, Outlined::On, "Gradient PlayerName"};
```

---

## Glows

```cpp
struct Glow {
  Color color;
  float radius = 4.f;
  float intensity = 1.f;
};

struct CanvasGlow {
  Glow  glow;
  int   layers = 64;
  float rounding = 20.f;
};
```

* `Glow` is accepted (as `std::optional<Glow>`) by `Box`, `CorneredBox`, `Bar`, `Label`. It is drawn as `ceil(radius)` stacked, progressively fainter strokes (or, for text, offset copies of the text) behind the widget. A non-positive radius draws nothing.
* `CanvasGlow` is a standalone widget for `EntityOverlay` and the `glow` member of `CenterBar`. It draws a soft bloom around the whole canvas rectangle: `layers` rounded-rectangle strokes with a smoothstep falloff, clipped so the glow never leaks into the interior. `rounding` is clamped to half of the shorter side. It relies on `HudRendererInterface::add_polyline_clipped`; a backend that does not override it still draws, but the glow is not clipped.

---

## Entity-box widgets

```cpp
struct Box {
  Color color;
  Paint fill{Color{0,0,0,0}};
  Color outline{0,0,0,0};
  float thickness = 1.f;
  std::optional<Glow> glow = std::nullopt;
};

struct CorneredBox {
  Color color;
  Color fill{0,0,0,0};
  Color outline{0,0,0,0};
  float corner_ratio = 0.2f;     // each corner arm = box width * corner_ratio
  float thickness = 1.f;
  std::optional<Glow> glow = std::nullopt;
};

struct DashedBox {
  Color color;
  float dash_len = 8.f;
  float gap_len = 5.f;
  float thickness = 1.f;
};
```

An alpha of `0` on `fill` or `outline` means "skip it". `Box::fill` is a `Paint` (solid or gradient); `CorneredBox::fill` is a plain `Color`. When `outline` is visible it is drawn as a line `2` px thicker than `thickness` under the main line.

```cpp
struct Bone { Vector2<float> start; Vector2<float> end; };

struct Skeleton {
  std::vector<Bone> bones;
  Color color;
  float thickness = 1.f;

  Skeleton(const Color& color, float thickness = 1.f);
  Skeleton(std::span<const Bone> bones, const Color& color, float thickness = 1.f);
  Skeleton(std::initializer_list<Bone> bones, const Color& color, float thickness = 1.f);
  template<std::size_t N>
  Skeleton(const std::array<Bone, N>& bones, const Color& color, float thickness = 1.f);
};
```

`Skeleton` with no bones draws a built-in 15-segment humanoid figure scaled to the canvas box. With bones, each `Bone` is a line in absolute screen coordinates (typically the projected joints of the entity).

```cpp
struct SnapLine    { Vector2<float> start; Color color; float width; };   // start -> bottom-centre of the box
struct ScanMarker  { Color color; Color outline{0,0,0,0}; float outline_thickness = 1.f; };  // triangle near the head
struct AimDot      { Vector2<float> position; Color color; float radius = 3.f; };            // absolute screen position

struct ProjectileAim {
  enum class Figure { CIRCLE, SQUARE };
  Vector2<float> position;       // absolute screen position of the predicted aim point
  Color color;
  float size = 3.f;
  float line_size = 1.f;         // thickness of the line from box centre to `position`
  Figure figure = Figure::SQUARE;
};
```

---

## Side-container widgets

`RightSide`, `LeftSide`, `TopSide` and `BottomSide` take an initializer list of `SideWidget`:

```cpp
using SideWidget = std::variant<None, Bar, DashedBar, Label, Centered<Label>,
                                SpaceVertical, SpaceHorizontal, ProgressRing, Icon>;

struct RightSide  { RightSide (std::initializer_list<SideWidget>); };
struct LeftSide   { LeftSide  (std::initializer_list<SideWidget>); };
struct TopSide    { TopSide   (std::initializer_list<SideWidget>); };
struct BottomSide { BottomSide(std::initializer_list<SideWidget>); };
```

Children are drawn in order, each advancing that side's cursor so the next one stacks outward from the box.

```cpp
struct Bar {                       // `size` is width for Left/Right, height for Top/Bottom
  BarPaint color; Color outline; Color bg;
  float size; float ratio; float offset = 5.f;
  std::optional<Glow> glow = std::nullopt;
};

struct DashedBar {                 // Bar without glow, with a dash pattern punched in
  BarPaint color; Color outline; Color bg;
  float size; float ratio; float dash_len; float gap_len; float offset = 5.f;
};

enum class Outlined { Off, On };

struct Label {
  Paint color;
  float offset;
  Outlined outlined;
  std::string_view text;
  std::optional<Glow> glow = std::nullopt;
};

template<typename W> struct Centered { W child; };   // Centered{Label{...}}

struct SpaceVertical   { float size; };
struct SpaceHorizontal { float size; };

struct ProgressRing {
  Color color;
  Color bg{0.3f, 0.3f, 0.3f, 0.5f};
  float radius = 12.f;
  float ratio;
  float thickness = 2.f;
  float offset = 5.f;
  int segments = 32;
};

struct Icon {
  std::any texture_id;           // renderer specific, e.g. ImTextureID for ImGui
  float width; float height;
  Color tint{1,1,1,1};
  float offset = 5.f;
};

struct None {};                  // no-op placeholder
```

Behaviour per side:

| Side | Bars | Labels / rings / icons | Spacers |
|------|------|------------------------|---------|
| `RightSide` | Stack to the right of the box, growing bottom-to-top | Stack downward from the top-right corner, text left-aligned at `cursor.x + offset` | `SpaceVertical` moves down, `SpaceHorizontal` moves right |
| `LeftSide` | Stack to the left of the box, growing bottom-to-top | Stack downward from the top-left corner, text right-aligned to the cursor | `SpaceVertical` moves down, `SpaceHorizontal` moves left |
| `TopSide` | Stack upward above the box, as wide as the box, growing left-to-right | Stack upward; `Centered<Label>` centres on the box | `SpaceVertical` moves up, `SpaceHorizontal` shifts the x cursor right |
| `BottomSide` | Stack downward below the box, as wide as the box, growing left-to-right | Stack downward; `Centered<Label>` centres on the box | `SpaceVertical` moves down, `SpaceHorizontal` shifts the x cursor right |

`Centered<Label>` only changes placement in `TopSide` and `BottomSide`; in `RightSide` and `LeftSide` it is drawn like a plain `Label`. `ratio` is clamped to `[0, 1]`.

---

## Screen widgets

Used with [`ScreenOverlay`](screen_overlay.md): `Crosshair`, `FovCircle`, `ThreatArrow`, `HitMarker`, `CenterBar`, `DashedCenterBar`, `Anchor`, `Corner`. Their fields are listed on that page.

---

## Conditional widgets

```cpp
namespace omath::hud::widget {
  // Inside side containers: SideWidget, or None when false
  template<typename W> requires std::constructible_from<SideWidget, W>
  SideWidget when(bool condition, W widget);
}
namespace omath::hud {
  // Top level: engaged optional or std::nullopt
  template<typename W>
  std::optional<W> when(bool condition, W widget);
}
```

```cpp
namespace widget = omath::hud::widget;
using omath::hud::when;

overlay.contents(
    RightSide{ widget::when(show_ping, Label{white, 2.f, Outlined::On, "42 ms"}) },
    when(show_box, Box{red}));
```

!!! note "Conditionals inside a Corner"
    `widget::when` returns a `SideWidget`, which is not convertible to `CornerWidget`, so it cannot be used for `Corner` children. Wrap the whole `Corner` with `hud::when` at the top level, or pick between `CornerWidget{Label{...}}` and `CornerWidget{None{}}` yourself.

`Label::text` is a `std::string_view`: it does not copy the string. Keep the characters alive until `contents()` returns (string literals, or a `std::string` that outlives the full expression).

---

## `CanvasBox`

```cpp
namespace omath::hud {
class CanvasBox final {
public:
  CanvasBox() = default;
  CanvasBox(Vector2<float> top, Vector2<float> bottom, float ratio = 4.f);

  std::array<Vector2<float>, 4> as_array() const;   // TL, TR, BR, BL

  Vector2<float> top_left_corner;
  Vector2<float> top_right_corner;
  Vector2<float> bottom_left_corner;
  Vector2<float> bottom_right_corner;
};
}
```

The 2D rectangle that `EntityOverlay` draws around. It is built from a top and a bottom point on screen: `bottom.x` is replaced by `top.x`, `height = |top.y - bottom.y|`, and the box extends `height / ratio` to each side of that vertical line, so its total width is `2 * height / ratio`. A larger `ratio` gives a narrower box.

---

## Drawing helpers (`widget_render.hpp`)

The overlays share these free functions; they are public, so a custom overlay can reuse them.

```cpp
namespace omath::hud {

void draw_label(HudRendererInterface&, const Vector2<float>& position, const widget::Paint&,
                widget::Outlined, const std::string_view& text, const std::optional<widget::Glow>&);

void draw_glow_polyline(HudRendererInterface&, std::span<const Vector2<float>> points,
                        const widget::Glow&, float thickness);
void draw_glow_rectangle(HudRendererInterface&, const Vector2<float>& min, const Vector2<float>& max,
                         const std::optional<widget::Glow>&);
void draw_edge_glow(HudRendererInterface&, const Vector2<float>& min, const Vector2<float>& max,
                    const widget::CanvasGlow&);

void draw_filled_rectangle(HudRendererInterface&, const Vector2<float>& min, const Vector2<float>& max,
                           const widget::Paint&);
void draw_filled_circle(HudRendererInterface&, const Vector2<float>& center, float radius,
                        const widget::Paint&, int segments);

[[nodiscard]] widget::Paint resolve_bar_paint(const widget::BarPaint&, float ratio, bool vertical);

void draw_dash_gaps(HudRendererInterface&, const Vector2<float>& origin, const Vector2<float>& step_dir,
                    const Vector2<float>& perp_dir, float full_len, const Color& gap_color,
                    float dash_len, float gap_len);

}
```

| Function | Behaviour |
|----------|-----------|
| `draw_label` | Optional glow halo, then (if `Outlined::On`) eight black copies offset by one pixel, then the text itself, using `add_text` for a `Color` and `add_gradient_text` for a `Gradient` |
| `draw_glow_polyline` | Closed polyline stroked `ceil(radius)` times, each pass `2` px thicker and fainter |
| `draw_glow_rectangle` | `draw_glow_polyline` around a rectangle; no-op if the glow is unset or `radius <= 0` |
| `draw_edge_glow` | The `CanvasGlow` bloom; no-op if `glow.radius <= 0` |
| `draw_filled_rectangle` | `add_filled_rectangle` or `add_gradient_rectangle`; `min`/`max` may be given in any order |
| `draw_filled_circle` | `add_filled_circle` (skipped when the colour is fully transparent) or `add_gradient_filled_circle` |
| `resolve_bar_paint` | `Color`/`Gradient` pass through; `BarGradient` becomes a two-stop gradient from `empty_color` to the ratio-blended colour, oriented vertically (`vertical == true`) or horizontally |
| `draw_dash_gaps` | Paints `gap_color` rectangles over an already-drawn bar to create the dash pattern. Dashes are centred in `full_len` with any remainder split between a leading and trailing gap |

---

## See also

* [HUD overview](overview.md)
* [EntityOverlay](entity_overlay.md)
* [ScreenOverlay](screen_overlay.md)
* [HudRendererInterface](renderer_interface.md)
* [Color](../utility/color.md)
