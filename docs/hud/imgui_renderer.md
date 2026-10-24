# `omath::hud::ImguiHudRenderer` — ImGui backend for the HUD

> Header: `omath/hud/renderer_realizations/imgui_renderer.hpp`
> Namespace: `omath::hud`
> Purpose: implements [`HudRendererInterface`](renderer_interface.md) on top of ImGui's background draw list
> Requires: the `OMATH_IMGUI_INTEGRATION` build option (the whole class is compiled out without it)

---

## Requirements

`ImguiHudRenderer` exists only when omath is built with ImGui integration. The header body and `imgui_renderer.cpp` are both wrapped in `#ifdef OMATH_IMGUI_INTEGRATION`; without the macro, including the header gives you an empty translation unit and `ImguiHudRenderer` is undeclared.

| Build system | How to enable |
|--------------|---------------|
| CMake | `-DOMATH_IMGUI_INTEGRATION=ON` |
| vcpkg | feature `imgui` (`omath[imgui]`), or `all` which also pulls in `lua` and `hooking` |

With the option on, CMake defines `OMATH_IMGUI_INTEGRATION` publicly on the `omath` target and links ImGui publicly: an existing `imgui` target (for example from a submodule) is used if present, otherwise `find_package(imgui CONFIG REQUIRED)` and `imgui::imgui`. Your own code therefore sees the macro automatically when it links `omath::omath`.

The same option also turns on `Vector2::to_im_vec2()` / `from_im_vec2()`, `Vector4::to_im_vec4()` and `Color::to_im_color()`, which the renderer itself uses. They are covered by `tests/general/unit_test_imgui_intergration.cpp`.

You still own the ImGui side: create the context, initialise a platform/renderer backend (GLFW + OpenGL3, Win32 + DX11, ...), and call `NewFrame`/`Render` yourself.

---

## API

```cpp
#ifdef OMATH_IMGUI_INTEGRATION
namespace omath::hud {

class ImguiHudRenderer final : public HudRendererInterface {
public:
  ~ImguiHudRenderer() override;

  void add_line(...) override;
  void add_polyline(...) override;
  void add_polyline_clipped(...) override;
  void add_filled_polyline(...) override;
  void add_rectangle(...) override;
  void add_filled_rectangle(...) override;
  void add_gradient_rectangle(...) override;
  void add_circle(...) override;
  void add_filled_circle(...) override;
  void add_gradient_filled_circle(...) override;
  void add_arc(...) override;
  void add_image(...) override;
  void add_text(...) override;
  void add_gradient_text(...) override;
  Vector2<float> calc_text_size(const std::string_view& text) override;
};

}
#endif
```

It has a default constructor and no state, so `std::make_shared<omath::hud::ImguiHudRenderer>()` is all that is needed. It overrides every optional gradient/clipping method, so gradients and glows render fully.

---

## Behavior

| Method | ImGui call |
|--------|------------|
| `add_line` | `ImDrawList::AddLine` |
| `add_polyline` | `AddPolyline` with `ImDrawFlags_Closed` |
| `add_polyline_clipped` | `PushClipRect(clip_min, clip_max, true)`, closed `AddPolyline`, `PopClipRect` |
| `add_filled_polyline` | `AddConvexPolyFilled` (convex shapes only) |
| `add_rectangle` / `add_filled_rectangle` | `AddRect` / `AddRectFilled` |
| `add_gradient_rectangle` | `AddRectFilledMultiColor` with the gradient's four corners |
| `add_circle` / `add_filled_circle` | `AddCircle` / `AddCircleFilled` |
| `add_arc` | `PathArcTo` + `PathStroke` |
| `add_image` | `AddImage`, `texture_id` must hold an `ImTextureID` |
| `add_text` | `AddText` with the current ImGui font |
| `calc_text_size` | `ImGui::CalcTextSize` |

Everything is drawn into `ImGui::GetBackgroundDrawList()`, so overlays render behind ImGui windows and need no window of their own. Call the overlays between `ImGui::NewFrame()` and `ImGui::Render()`.

### Gradients

* `add_gradient_rectangle` blends the four corners; `animated` has no effect on rectangles.
* `add_gradient_filled_circle` and `add_gradient_text` draw white geometry and then rewrite the vertex colours:
  * static: bilinear blend of the four corners across the shape's bounding box;
  * animated: blend between `top_left` and `top_right` using `sin(ImGui::GetTime() * animation_speed + direction * phase * animation_spread)`, where `phase` is the glyph index for text and the angle around the centre for circles. `GradientDirection::LeftToRight` reverses the sign.

---

## Usage

```cpp
#include <omath/hud/entity_overlay.hpp>
#include <omath/hud/renderer_realizations/imgui_renderer.hpp>
#include <omath/hud/screen_overlay.hpp>

// after ImGui_Impl*_NewFrame(); ImGui::NewFrame();
const auto renderer = std::make_shared<omath::hud::ImguiHudRenderer>();

const auto display = ImGui::GetIO().DisplaySize;

omath::hud::ScreenOverlay({display.x, display.y}, renderer)
    .contents(omath::hud::widget::Crosshair{omath::Color::green()});

// ... ImGui::Render(); ImGui_Impl*_RenderDrawData(ImGui::GetDrawData());
```

`examples/example_hud/gui/main_window.cpp` shows the complete frame loop, and `examples/example_dx9_hook`, `example_dx11_hook`, `example_dx12_hook` and `example_opengl_hook` show ImGui being driven from a [hooked present call](../hooks/hooks_manager.md), which is where this renderer is normally used.

---

## Notes

* **NUL-terminated text.** `calc_text_size` passes `text.data()` to `ImGui::CalcTextSize` without an end pointer, so it measures up to the first NUL, not up to `text.size()`. String literals and `std::string` are fine; a `string_view` slicing the middle of a larger buffer is not.
* **Texture type.** `add_image` does `std::any_cast<ImTextureID>(texture_id)`; passing any other type throws `std::bad_any_cast`.
* **Include path.** The renderer's source uses `imgui_internal.h` (for UTF-8 glyph iteration in animated gradient text), so the ImGui headers must be on the include path of the omath build.
* **Antialiasing.** Lines and fills follow ImGui's `AntiAliasedLines`/`AntiAliasedFill` style flags; `example_hud` disables both.
* If you do not want the ImGui dependency, implement [`HudRendererInterface`](renderer_interface.md) yourself.

---

## See also

* [HUD overview](overview.md)
* [HudRendererInterface](renderer_interface.md)
* [Hooks manager](../hooks/hooks_manager.md)
* [Color](../utility/color.md)
