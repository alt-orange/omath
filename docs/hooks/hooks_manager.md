# `omath::hooks::HooksManager` — automatic DirectX / OpenGL present hooks

> Header: `omath/hooks/hooks_manager.hpp`
> Namespace: `omath::hooks`
> Purpose: find and inline-hook the frame-present functions of DirectX 9/11/12 and OpenGL inside another process (typically from an injected DLL) and call your callbacks every frame, so you can draw an overlay such as the [HUD](../hud/overview.md)
> Requires: `OMATH_ENABLE_HOOKING` (CMake) / `hooking` (vcpkg feature)

---

## Requirements

| Item | Details |
|------|---------|
| CMake option | `-DOMATH_ENABLE_HOOKING=ON` (default `OFF`). Defines `OMATH_ENABLE_HOOKING` publicly on the `omath` target |
| vcpkg feature | `hooking` (`omath[hooking]`), also part of `all`. Depends on `safetyhook` |
| Hooking library | [safetyhook](https://github.com/cursey/safetyhook), found with `find_package(safetyhook CONFIG REQUIRED)` and used for inline hooks |
| Windows | Supported: DirectX 9, 11, 12, OpenGL (`wglSwapBuffers` and `SwapBuffers`) and a window-procedure hook. CMake links `d3d9 d3d11 d3d12 dxgi opengl32 gdi32` |
| Linux | Supported: OpenGL through `glXSwapBuffers` only. CMake links `OpenGL::GL` and `${CMAKE_DL_LIBS}` |
| Other platforms | vcpkg's `supports` for the feature is `(windows | linux) & !arm & !uwp`: no macOS, no ARM, no UWP |

The public header includes `<safetyhook.hpp>` (the hooks are class members), plus `<Windows.h>`, `<d3d9.h>`, `<d3d12.h>` and `<dxgi.h>` on Windows or `<GL/glx.h>` on Linux. Make sure those headers can be found in code that includes it.

!!! warning "Disabled build"
    When `OMATH_ENABLE_HOOKING` is not defined the header still compiles, but `HooksManager` is reduced to `static HooksManager& get()` plus a destructor. None of the `hook_*`/`set_on_*` functions below exist.

The repository's hook examples are only built when both `OMATH_ENABLE_HOOKING` and `OMATH_BUILD_EXAMPLES` are on, on Windows, with ImGui installed with the matching bindings:

```bash
vcpkg install imgui[dx9-binding,dx11-binding,dx12-binding,opengl3-binding,win32-binding]
```

---

## API

```cpp
namespace omath::hooks {

class HooksManager final {
public:
  [[nodiscard]] static HooksManager& get();         // process-wide singleton
  HooksManager(const HooksManager&) = delete;
  HooksManager& operator=(const HooksManager&) = delete;
  ~HooksManager();                                   // removes every hook it installed

  template<typename Callback> using callback_ptr = std::shared_ptr<const Callback>;

#ifdef _WIN32
  // DirectX 9
  using dx9_present_callback   = std::function<void(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*)>;
  using dx9_reset_callback     = std::function<void(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)>;
  using dx9_end_scene_callback = std::function<void(IDirect3DDevice9*)>;

  [[nodiscard]] bool hook_dx9();
  void unhook_dx9();
  void set_on_dx9_present(dx9_present_callback);
  void set_on_dx9_reset(dx9_reset_callback);
  void set_on_dx9_end_scene(dx9_end_scene_callback);

  // DirectX 11 and 12 share the IDXGISwapChain callbacks
  using present_callback         = std::function<void(IDXGISwapChain*, UINT, UINT)>;
  using resize_buffers_callback  = std::function<void(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)>;
  using execute_command_lists_callback =
        std::function<void(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*)>;

  [[nodiscard]] bool hook_dx11();
  void unhook_dx11();
  [[nodiscard]] bool hook_dx12();
  void unhook_dx12();
  void set_on_present(present_callback);
  void set_on_resize_buffers(resize_buffers_callback);
  void set_on_execute_command_lists(execute_command_lists_callback);   // DX12 only

  // Window procedure
  using wnd_proc_callback = std::function<std::optional<LRESULT>(HWND, UINT, WPARAM, LPARAM)>;

  [[nodiscard]] bool hook_wnd_proc(HWND hwnd);
  void unhook_wnd_proc();
  void set_on_wnd_proc(wnd_proc_callback);

  // OpenGL (Windows)
  using opengl_swap_buffers_callback = std::function<void(HDC)>;
#endif

#ifdef __linux__
  // OpenGL (Linux / GLX)
  using opengl_swap_buffers_callback = std::function<void(Display*, GLXDrawable)>;
#endif

  [[nodiscard]] bool hook_opengl();
  void unhook_opengl();
  void set_on_opengl_swap_buffers(opengl_swap_buffers_callback);
};

}
```

---

## Behavior

### General

* `get()` returns a function-local static. `hook_*` and `unhook_*` take an exclusive lock on the hook state; `hook_*` is **idempotent** and returns `true` immediately if that hook is already installed.
* `hook_*` returns `false` if the target API is not usable: its DLL is not loaded yet, the dummy device could not be created, or a hook failed to install (in that case any partial hooks are rolled back).
* `set_on_*` replaces the callback; passing an empty `std::function` clears it. Callbacks may be set before or after the hook is installed.
* **Callbacks run on the application's render thread, before the original function.** Every detour copies a `shared_ptr` to the callback under a shared lock, releases the lock, calls your callback, then calls the original. This means a callback may itself call `set_on_*` without deadlocking, and replacing a callback from another thread is safe.
* The destructor calls every `unhook_*`, but in an injected DLL you should unhook explicitly on `DLL_PROCESS_DETACH`, before tearing down your own resources.

### How the targets are found

The manager does not need offsets or signatures. For each API it creates a throw-away object, reads the function address from its vtable, and installs an inline hook there. Because the code behind a vtable is shared per module, the hook affects the real device the application uses.

| API | Needs loaded | Hooked functions |
|-----|--------------|------------------|
| DX9 | `d3d9.dll` | `IDirect3DDevice9::Reset` (vtable 16), `Present` (17), `EndScene` (42). Callbacks: `set_on_dx9_reset`, `set_on_dx9_present`, `set_on_dx9_end_scene` |
| DX11 | `d3d11.dll` | `IDXGISwapChain::Present` (8), `ResizeBuffers` (13). Callbacks: `set_on_present`, `set_on_resize_buffers` |
| DX12 | `d3d12.dll`, `dxgi.dll` | `IDXGISwapChain::Present`, `ResizeBuffers`, `ID3D12CommandQueue::ExecuteCommandLists` (10). Callbacks: `set_on_present`, `set_on_resize_buffers`, `set_on_execute_command_lists` |
| OpenGL (Windows) | `opengl32.dll` / `gdi32.dll` | `wglSwapBuffers` and `SwapBuffers`. `hook_opengl()` succeeds if at least one of the two could be hooked |
| OpenGL (Linux) | an already loaded GL library exporting `glXSwapBuffers` | `glXSwapBuffers`, resolved with `dlsym(RTLD_DEFAULT, ...)` |

Practical consequences:

* The relevant DLL must already be loaded in the process; otherwise `hook_*` returns `false`. The examples poll `GetModuleHandle("d3d11.dll")` (and so on) in a worker thread and hook once it appears.
* The DX11 and DX12 callbacks are the same `present_callback`/`resize_buffers_callback` objects and fire for whichever of the two is hooked. Hook one of them per process, as the examples do.
* DX12 overlays need a command queue to submit work. `set_on_execute_command_lists` lets you capture the game's direct queue as it executes command lists; `examples/example_dx12_hook` shows the full pattern.

### Window procedure hook

`hook_wnd_proc(hwnd)` replaces the window's `WndProc` with `SetWindowLongPtr`. The callback receives every message and returns:

* `std::nullopt` to let the original procedure handle the message, or
* an `LRESULT` to consume it (the original procedure is not called).

This is how ImGui receives mouse and keyboard input inside a game window. Only one window can be hooked at a time; `unhook_wnd_proc()` restores the original procedure. `hook_wnd_proc` returns `false` if `SetWindowLongPtr` fails.

---

## Usage

A condensed version of `examples/example_dx11_hook/dllmain.cpp` (build the target as a DLL and inject it into a DirectX 11 process):

```cpp
#include <omath/hooks/hooks_manager.hpp>
#include <Windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static bool g_ready = false;

static void on_present(IDXGISwapChain* swap_chain, UINT, UINT)
{
    if (!g_ready)
    {
        // First frame: create ImGui and the D3D11 objects from the swap chain.
        // ... swap_chain->GetDevice(...), ImGui::CreateContext(), ImGui_ImplDX11_Init(...) ...
        auto& mgr = omath::hooks::HooksManager::get();
        mgr.set_on_wnd_proc([](HWND h, UINT msg, WPARAM wp, LPARAM lp) -> std::optional<LRESULT>
        {
            if (ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp))
                return 0;                  // ImGui consumed it
            return std::nullopt;           // pass to the game
        });
        // mgr.hook_wnd_proc(output_window);
        g_ready = true;
        return;
    }

    // Each later frame: set the render target, NewFrame, draw, Render, RenderDrawData.
}

static void on_resize_buffers(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)
{
    // Release anything that references the back buffer before ResizeBuffers runs.
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        std::thread([]
        {
            while (!GetModuleHandle("d3d11.dll"))
                Sleep(100);

            auto& mgr = omath::hooks::HooksManager::get();
            mgr.set_on_present(on_present);
            mgr.set_on_resize_buffers(on_resize_buffers);
            (void)mgr.hook_dx11();
        }).detach();
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        auto& mgr = omath::hooks::HooksManager::get();
        mgr.unhook_wnd_proc();
        mgr.unhook_dx11();
        // shut down ImGui and release your D3D objects
    }
    return TRUE;
}
```

Drawing the [HUD](../hud/overview.md) from `on_present` is just a matter of creating an `ImguiHudRenderer` and building overlays between `ImGui::NewFrame()` and `ImGui::Render()`; see [ImguiHudRenderer](../hud/imgui_renderer.md).

The four examples differ mainly in what they must do to survive the API:

| Example | Callbacks used | Notes |
|---------|----------------|-------|
| `example_dx9_hook` | `set_on_dx9_present`, `set_on_dx9_reset` | Calls `ImGui_ImplDX9_InvalidateDeviceObjects` in the reset callback |
| `example_dx11_hook` | `set_on_present`, `set_on_resize_buffers` | Releases the render-target view on resize, restores the game's render targets after drawing |
| `example_dx12_hook` | `set_on_present`, `set_on_resize_buffers`, `set_on_execute_command_lists` | Captures the direct command queue, owns per-frame allocators, a fence and an SRV heap, `INSERT` toggles the menu |
| `example_opengl_hook` | `set_on_opengl_swap_buffers` | Uses `WindowFromDC(hdc)` to find the window, `INSERT` toggles the menu |

---

## Notes

* **Initialise lazily.** `DllMain` runs under the loader lock; do not create ImGui or graphics objects there. Do it from the first present/swap callback, as all examples do, using the device/swap chain handed to the callback.
* **Hook order.** Install callbacks first, then call `hook_*`, so the first frame already reaches your code.
* **One owner per vtable function.** Do not install your own inline hook on the same function through another library at the same time.
* **Build flag drift.** If your project links an `omath` built without hooking but compiles your sources with `OMATH_ENABLE_HOOKING` defined (or vice versa), the class layouts differ. Take the definition from the `omath::omath` target, which exports it.
* On Linux only `hook_opengl`, `unhook_opengl` and `set_on_opengl_swap_buffers` exist, and the callback signature is `(Display*, GLXDrawable)`.

---

## See also

* [HUD overview](../hud/overview.md)
* [ImguiHudRenderer](../hud/imgui_renderer.md)
* [Install](../install.md)
