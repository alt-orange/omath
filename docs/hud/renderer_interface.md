# `omath::hud::HudRendererInterface` — backend contract for the HUD

> Header: `omath/hud/hud_renderer_interface.hpp`
> Namespace: `omath::hud`
> Purpose: the abstract set of 2D drawing primitives that [`EntityOverlay`](entity_overlay.md) and [`ScreenOverlay`](screen_overlay.md) use; implement it to draw the HUD with any graphics API

---

## API

```cpp
namespace omath::hud {

class HudRendererInterface {
public:
  virtual ~HudRendererInterface() = default;

  // ── Required ──────────────────────────────────────────────────────────────
  virtual void add_line(const Vector2<float>& line_start, const Vector2<float>& line_end,
                        const Color& color, float thickness) = 0;

  virtual void add_polyline(const std::span<const Vector2<float>>& vertexes,
                            const Color& color, float thickness) = 0;       // closed outline

  virtual void add_filled_polyline(const std::span<const Vector2<float>>& vertexes,
                                   const Color& color) = 0;                 // convex fill

  virtual void add_rectangle(const Vector2<float>& min, const Vector2<float>& max,
                             const Color& color) = 0;                       // outline

  virtual void add_filled_rectangle(const Vector2<float>& min, const Vector2<float>& max,
                                    const Color& color) = 0;

  virtual void add_circle(const Vector2<float>& center, float radius, const Color& color,
                          float thickness, int segments = 0) = 0;

  virtual void add_filled_circle(const Vector2<float>& center, float radius, const Color& color,
                                 int segments = 0) = 0;

  // Angles in radians, 0 = right (+X)
  virtual void add_arc(const Vector2<float>& center, float radius, float a_min, float a_max,
                       const Color& color, float thickness, int segments = 0) = 0;

  // texture_id is renderer specific (e.g. ImTextureID for ImGui)
  virtual void add_image(const std::any& texture_id, const Vector2<float>& min,
                         const Vector2<float>& max, const Color& tint = Color{1,1,1,1}) = 0;

  virtual void add_text(const Vector2<float>& position, const Color& color,
                        const std::string_view& text) = 0;

  [[nodiscard]]
  virtual Vector2<float> calc_text_size(const std::string_view& text) = 0;

  // ── Optional, with fallbacks ──────────────────────────────────────────────
  virtual void add_polyline_clipped(const std::span<const Vector2<float>>& vertexes,
                                    const Vector2<float>& clip_min, const Vector2<float>& clip_max,
                                    const Color& color, float thickness);       // -> add_polyline

  virtual void add_gradient_rectangle(const Vector2<float>& min, const Vector2<float>& max,
                                      const Gradient& gradient);                // -> add_filled_rectangle(top_left)

  virtual void add_gradient_filled_circle(const Vector2<float>& center, float radius,
                                          const Gradient& gradient, int segments = 0);  // -> add_filled_circle(top_left)

  virtual void add_gradient_text(const Vector2<float>& position, const Gradient& gradient,
                                 const std::string_view& text);                 // -> add_text(top_left)
};

}
```

---

## Behavior

* **Coordinates** are screen pixels with the origin at the top-left and `y` growing downward, matching the output of `Camera::world_to_screen`.
* **Colours** are `omath::Color` (RGBA, each channel in `[0, 1]`).
* **Required vs optional.** The eleven pure virtuals are the minimum a backend needs. The four optional methods have default implementations that degrade gracefully, so a minimal backend still draws gradients as a flat `top_left` colour and glows unclipped.
* **Who calls what.** `Box`/`CorneredBox`/`DashedBox` use lines, polylines and rectangles; bars use filled/outlined rectangles and (for dashes) filled rectangles; labels use `add_text` (or `add_gradient_text`) and `calc_text_size`; `ProgressRing`, `FovCircle` and the aim markers use circles and arcs; `Icon` uses `add_image`; `CanvasGlow`/`CenterBar` glow use `add_polyline_clipped`.
* **Ownership.** Overlays hold the renderer as `std::shared_ptr<HudRendererInterface>`. Implementations may be stateless (as `ImguiHudRenderer` is) or hold a draw list/batch that you flush after the overlays have run.
* **Threading.** The interface does no synchronisation. Call it from the thread that owns the underlying graphics context.

!!! note "add_polyline_clipped"
    The clip rectangle is used by the glow bloom to stop it bleeding into the interior of a box or bar. If your backend can scissor a polyline, override this method; otherwise the default (which ignores the clip rectangle) is fine for everything except the `CanvasGlow` look.

---

## Implementing a backend

```cpp
#include <omath/hud/hud_renderer_interface.hpp>

class MyRenderer final : public omath::hud::HudRendererInterface
{
public:
    void add_line(const omath::Vector2<float>& a, const omath::Vector2<float>& b,
                  const omath::Color& color, float thickness) override
    {
        m_batch.line(a.x, a.y, b.x, b.y, color.value(), thickness);   // color.value() is a Vector4<float> r,g,b,a
    }

    void add_polyline(const std::span<const omath::Vector2<float>>& points,
                      const omath::Color& color, float thickness) override
    {
        for (std::size_t i = 0; i < points.size(); ++i)                // closed loop
            add_line(points[i], points[(i + 1) % points.size()], color, thickness);
    }

    void add_filled_polyline(const std::span<const omath::Vector2<float>>& points,
                             const omath::Color& color) override { m_batch.convex_fill(points, color.value()); }

    void add_rectangle(const omath::Vector2<float>& min, const omath::Vector2<float>& max,
                       const omath::Color& color) override { m_batch.rect(min, max, color.value()); }
    void add_filled_rectangle(const omath::Vector2<float>& min, const omath::Vector2<float>& max,
                              const omath::Color& color) override { m_batch.filled_rect(min, max, color.value()); }

    void add_circle(const omath::Vector2<float>& c, float r, const omath::Color& color,
                    float thickness, int segments) override { /* ... */ }
    void add_filled_circle(const omath::Vector2<float>& c, float r, const omath::Color& color,
                           int segments) override { /* ... */ }
    void add_arc(const omath::Vector2<float>& c, float r, float a_min, float a_max,
                 const omath::Color& color, float thickness, int segments) override { /* ... */ }
    void add_image(const std::any& texture_id, const omath::Vector2<float>& min,
                   const omath::Vector2<float>& max, const omath::Color& tint) override { /* ... */ }

    void add_text(const omath::Vector2<float>& position, const omath::Color& color,
                  const std::string_view& text) override { m_font.draw(position, color.value(), text); }

    omath::Vector2<float> calc_text_size(const std::string_view& text) override { return m_font.measure(text); }

private:
    MyBatch m_batch;
    MyFont m_font;
};

auto renderer = std::make_shared<MyRenderer>();
omath::hud::ScreenOverlay({1920.f, 1080.f}, renderer).contents(/* ... */);
```

`m_batch` and `m_font` stand for your own drawing code. The Lua bindings contain a complete example of a non-ImGui backend: `LuaHudRenderer` in `source/lua/lua_hud.cpp` forwards each call to a Lua callback table (see [Lua](../lua/lua.md)).

---

## Notes

* `calc_text_size` must agree with `add_text`: overlays use it to right-align text on the left side, to centre labels, and to advance the vertical cursor by one line.
* Default arguments (`segments = 0`, `tint`) are bound to the static type. Overlays call through `HudRendererInterface`, so the defaults declared on the interface are the ones that apply; they are passed to your override as ordinary arguments.
* A renderer is not required to support `add_image` textures of any particular type; the `std::any` payload is passed through unchanged from `Icon::texture_id` / `add_*_icon`.

---

## See also

* [HUD overview](overview.md)
* [ImguiHudRenderer](imgui_renderer.md) — the bundled implementation
* [Widgets](widgets.md)
* [Color](../utility/color.md)
* [Vector2](../linear_algebra/vector2.md)
