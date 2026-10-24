# `omath::hud` — declarative HUD / ESP overlay system

> Headers: `omath/hud/*.hpp`, `omath/hud/renderer_realizations/imgui_renderer.hpp`
> Namespaces: `omath::hud`, `omath::hud::widget`
> Purpose: describe what to draw around an entity (boxes, bars, labels, skeletons) or on the screen (crosshair, FOV circle, corner text) as a list of small value-type widgets; a renderer backend turns them into draw calls

---

## Layers

```text
 your code ──► widget:: descriptors ──► EntityOverlay / ScreenOverlay ──► HudRendererInterface ──► ImGui / your backend
 (data)        (plain structs)          (layout + drawing math)            (primitives only)
```

| Layer | Page | Role |
|-------|------|------|
| Widgets | [Widgets](widgets.md) | Plain structs (`Box`, `Bar`, `Label`, `Crosshair`, ...), paints (`Color` / `Gradient` / `BarGradient`), glows, `when()` |
| Entity overlay | [EntityOverlay](entity_overlay.md) | Lays widgets out around the 2D box of one entity |
| Screen overlay | [ScreenOverlay](screen_overlay.md) | Lays widgets out relative to the screen centre and corners |
| Renderer interface | [HudRendererInterface](renderer_interface.md) | The 11 primitive operations a backend has to provide |
| ImGui backend | [ImguiHudRenderer](imgui_renderer.md) | Ready-made backend drawing into ImGui's background draw list |

Overlays never talk to a graphics API. They only call `HudRendererInterface`, so the same widget tree can be drawn through ImGui, a custom D3D/GL batcher, or a Lua callback table (see [Lua](../lua/lua.md)).

---

## Declarative composition

Both overlays expose a variadic `contents(...)` method. Pass widget structs in the order you want them drawn; the overlay dispatches each one to the matching drawing routine. Side containers (`LeftSide`, `RightSide`, `TopSide`, `BottomSide`) group widgets that stack outward from one edge of the entity box, and `when(condition, widget)` makes a widget conditional without an `if` around the call.

```cpp
#include <omath/hud/entity_overlay.hpp>
#include <omath/hud/screen_overlay.hpp>
#include <omath/hud/renderer_realizations/imgui_renderer.hpp>

namespace widget = omath::hud::widget;
using namespace omath::hud::widget;
using omath::Color;
using omath::hud::when;

const auto renderer = std::make_shared<omath::hud::ImguiHudRenderer>();

// One entity: project its AABB, then describe the overlay.
if (auto overlay = omath::hud::EntityOverlay::from_aabb(camera, aabb, /*aspect*/ 4.f, renderer))
{
    const Color black{0.f, 0.f, 0.f, 1.f};
    const Color panel{0.1f, 0.1f, 0.1f, 0.8f};

    overlay->contents(
        CorneredBox{Color::red(), Color{0.f, 0.f, 0.f, 0.2f}, black},

        LeftSide{
            Bar{BarPaint{BarGradient{Color::red(), Color::green()}}, black, panel, 4.f, health_ratio},
        },

        RightSide{
            Label{Color::green(), 2.f, Outlined::On, "Health: 87"},
            widget::when(has_armor, Label{Color{0.f, 0.8f, 1.f, 1.f}, 2.f, Outlined::On, "Armor: 50"}),
            SpaceVertical{6.f},
            ProgressRing{Color::green(), panel, 10.f, reload_ratio},
        },

        TopSide{
            Centered{Label{Color{1.f, 1.f, 1.f, 1.f}, 2.f, Outlined::On, player_name}},
        },

        when(draw_skeleton, Skeleton{bones, Color{1.f, 1.f, 1.f, 1.f}, 1.f}),
        when(is_visible, SnapLine{{screen_w / 2.f, screen_h}, Color::green(), 1.f}));
}

// Once per frame: things that belong to the screen, not to an entity.
omath::hud::ScreenOverlay({screen_w, screen_h}, renderer)
    .contents(
        Crosshair{Color::green(), /*gap*/ 4.f, /*length*/ 8.f, /*thickness*/ 1.f},
        when(aimbot_on, FovCircle{Color::red(), fov_radius_px}),
        Corner{Anchor::TOP_LEFT, {Label{Color::green(), 0.f, Outlined::On, "omath::hud"}}});
```

Things worth knowing about this style:

* **Order is draw order.** `contents(a, b, c)` renders `a`, then `b`, then `c`.
* **Side containers keep a cursor.** Each `RightSide`/`LeftSide` child advances a vertical cursor (labels, rings, spacers) or horizontal cursor (bars) away from the box, so you never compute positions by hand.
* **`when` has two forms.** At the top level, `omath::hud::when(cond, w)` returns `std::optional<W>`, which `contents()` skips when empty. Inside a side container, `omath::hud::widget::when(cond, w)` returns a `SideWidget` that is `None` when the condition is false. See [Widgets](widgets.md#conditional-widgets).
* **Containers are non-owning.** Side containers and `Corner` hold a `std::initializer_list`, and `Label::text` is a `std::string_view`. Build them inside the `contents(...)` call (one full-expression) rather than storing them in variables.
* **An imperative API exists too.** Most widgets have a matching `add_*` method on the overlay (`add_cornered_2d_box`, `add_right_bar`, `add_top_label`, ...), and `contents()` calls them underneath. The `add_*` form is what the Lua bindings expose; `contents()` is the more composable choice in C++.

---

## Requirements

| Piece | Needs |
|-------|-------|
| Widgets, overlays, `HudRendererInterface`, `CanvasBox`, `Gradient` | Nothing beyond omath itself; always built |
| `ImguiHudRenderer` | `OMATH_IMGUI_INTEGRATION` (CMake option) / `imgui` vcpkg feature, plus an ImGui context and backend in your application. See [ImguiHudRenderer](imgui_renderer.md) |
| Lua bindings for `EntityOverlay`/`CanvasBox`/`Renderer` | `OMATH_ENABLE_LUA`. See [Lua](../lua/lua.md) |

---

## Examples in the repository

* `examples/example_hud` — desktop GLFW + ImGui window with a control panel for every widget (`gui/main_window.cpp` builds full `EntityOverlay` and `ScreenOverlay` trees in this style, with gradients, glows and animated paints). Built with `OMATH_BUILD_EXAMPLES`.
* `examples/example_kf1_overlay` — a transparent click-through overlay for a game. It projects world positions with omath's Unreal `Camera` and draws markers and a radar with ImGui's draw list directly; it is a good reference for window setup and camera input, but it does not go through the HUD widget classes.
* `tests/general/unit_test_entity_overlay.cpp` — `EntityOverlay::from_aabb` behaviour.
* `tests/lua/hud_tests.lua` — scripted overlay tests using a recording renderer.

---

## See also

* [Widgets](widgets.md)
* [EntityOverlay](entity_overlay.md)
* [ScreenOverlay](screen_overlay.md)
* [HudRendererInterface](renderer_interface.md)
* [ImguiHudRenderer](imgui_renderer.md)
* [Camera](../projection/camera.md) — produces the screen positions the overlays are anchored to
