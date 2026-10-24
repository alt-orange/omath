# Installation Guide

OMath is a C++23 library. It needs a compiler with C++23 support (the project documents Clang 18+, GCC 14+ or MSVC 19.38+) and CMake 3.26 or newer when you build it yourself.

Vectors, matrices, angles, cameras and the engine traits are header-only templates, but the pattern scanners, the AVX2 projectile engine, pathfinding, the HUD, the hooks and the Lua bindings have a compiled part, so you always link against the `omath` target.

## <img width="28px" src="https://vcpkg.io/assets/mark/mark.svg" /> Using vcpkg (recommended)
**Note**: Support vcpkg for package management
1. Install [vcpkg](https://github.com/microsoft/vcpkg)
2. Run the following command to install the orange-math package:
```bash
vcpkg install orange-math
```
CMakeLists.txt
```cmake
find_package(omath CONFIG REQUIRED)
target_link_libraries(main PRIVATE omath::omath)
```
For detailed commands on installing different versions and more information, please refer to Microsoft's [official instructions](https://learn.microsoft.com/en-us/vcpkg/get_started/overview).

Optional parts of the library are vcpkg features (defined in the repository's `vcpkg.json`), for example:

```bash
vcpkg install orange-math[imgui,lua,hooking]
```

Run `vcpkg search orange-math` to see which features your port version exposes, and see [Optional features](#optional-features) for what each one does.

## <img width="28px" src="https://xmake.io/assets/img/logo.svg" /> Using xrepo
**Note**: Support xrepo for package management
1. Install [xmake](https://xmake.io/)
2. Run the following command to install the omath package:
```bash
xrepo install omath
```
xmake.lua
```xmake
add_requires("omath")
target("...")
    add_packages("omath")
```

## <img width="28px" src="https://conan.io/favicon.png" /> Using Conan
**Note**: Support Conan for package management
1. Install [Conan](https://conan.io/downloads)
2. Run the following command to install the omath package:
```bash
conan install --requires="omath/[*]" --build=missing
```
conanfile.txt
```ini
[requires]
omath/[*]

[generators]
CMakeDeps
CMakeToolchain
```
CMakeLists.txt
```cmake
find_package(omath CONFIG REQUIRED)
target_link_libraries(main PRIVATE omath::omath)
```
For more details, see the [Conan documentation](https://docs.conan.io/2/).

## <img width="28px" src="https://github.githubassets.com/favicons/favicon.svg" /> Using prebuilt binaries (GitHub Releases)

**Note**: This is the fastest option if you don’t want to build from source.

1. **Go to the Releases page**
    - Open the project’s GitHub **Releases** page and choose the latest version.

2. **Download the correct asset for your platform**
    - Pick the archive that matches your OS and architecture (for example: Windows x64 / Linux x64 / macOS arm64).

3. **Extract the archive**
    - You should end up with something like:
        - `include/` (headers)
        - `lib/` or `bin/` (library files / DLLs)
        - sometimes `cmake/` (CMake package config)

4. **Use it in your project**

   ### Option A: CMake package (recommended if the release includes CMake config files)
   If the extracted folder contains something like `lib/cmake/omath` or `cmake/omath`, you can point CMake to it:

   ```cmake
   # Example: set this to the extracted prebuilt folder
   list(APPEND CMAKE_PREFIX_PATH "path/to/omath-prebuilt")

   find_package(omath CONFIG REQUIRED)
   target_link_libraries(main PRIVATE omath::omath)
   ```

   ### Option B: Manual include + link (works with any layout)
   If there’s no CMake package config, link it manually:

   ```cmake
   target_include_directories(main PRIVATE "path/to/omath-prebuilt/include")

   # Choose ONE depending on what you downloaded:
   # - Static library: .lib / .a
   # - Shared library: .dll + .lib import (Windows), .so (Linux), .dylib (macOS)

   target_link_directories(main PRIVATE "path/to/omath-prebuilt/lib")
   target_link_libraries(main PRIVATE omath)  # or the actual library filename
   ```

!!! note
    A prebuilt package is compiled with one fixed set of options. If your code uses the optional [HUD ImGui renderer](hud/imgui_renderer.md), the [hooks](hooks/hooks_manager.md) or the [Lua bindings](lua/lua.md), the package has to be built with the matching option (`OMATH_IMGUI_INTEGRATION`, `OMATH_ENABLE_HOOKING`, `OMATH_ENABLE_LUA`). Otherwise build from source.

## <img width="28px" src="https://upload.wikimedia.org/wikipedia/commons/e/ef/CMake_logo.svg?" /> Build from source using CMake
1. **Preparation**

   Install needed tools: cmake, clang, git, msvc (windows only).

    1. **Linux:**
        ```bash
        sudo pacman -Sy cmake ninja clang git
        ```
    2. **MacOS:**
        ```bash
        brew install llvm git cmake ninja
        ```
    3. **Windows:**

       Install Visual Studio from [here](https://visualstudio.microsoft.com/downloads/) and Git from [here](https://git-scm.com/downloads).

       Use x64 Native Tools shell to execute needed commands down below.
2. **Clone the repository:**
   ```bash
   git clone https://github.com/orange-cpp/omath.git
   ```
3. **Navigate to the project directory:**
   ```bash
   cd omath
   ```
4. **Build the project using CMake:**
   ```bash
   cmake --preset windows-release -S .
   cmake --build cmake-build/build/windows-release --target omath -j 6
   ```
   Use **\<platform\>-\<build configuration\>** preset to build suitable version for yourself. Like **windows-release** or **linux-release**.

   | Platform Name | Build Config  |
   |---------------|---------------|
   | windows       | release/debug |
   | linux         | release/debug |
   | darwin        | release/debug |

   `CMakePresets.json` also defines `-vcpkg` variants of these presets (for example `linux-release-vcpkg`) that take their dependencies from vcpkg, plus presets for other targets (x86, arm64, Android, iOS, wasm, MinGW, FreeBSD). The vcpkg presets need the `VCPKG_ROOT` environment variable.

## Using OMath as a CMake subproject

If you vendor the sources (git submodule, copy), add the directory and link the same alias target:

```cmake
set(OMATH_USE_AVX2 ON)               # optional, set options before add_subdirectory
add_subdirectory(third_party/omath)
target_link_libraries(main PRIVATE omath::omath)
```

The target exports `cxx_std_23` and the `OMATH_*` compile definitions publicly, so your code sees the same configuration as the library.

## Optional features

The options are CMake cache variables. When you use vcpkg, the matching feature of the `orange-math` port switches the option on for you.

| CMake option | vcpkg feature | Default | What it does |
|--------------|---------------|---------|--------------|
| `OMATH_BUILD_TESTS` | `tests` | `OFF` | Build the GoogleTest unit tests (`unit_tests` target). GoogleTest must be discoverable. |
| `OMATH_BUILD_BENCHMARK` | `benchmark` | `OFF` | Build the benchmarks (Google Benchmark). |
| `OMATH_BUILD_EXAMPLES` | `examples` | `OFF` | Build the projects in `examples/`. |
| `OMATH_USE_AVX2` | `avx2` | `ON` when the compiler supports AVX2 | Enables the AVX2 code paths (matrix multiply, pattern scan, `ProjPredEngineAvx2`). Turned off again, with a warning, when the compiler cannot target AVX2. The vcpkg feature is not available on ARM. |
| `OMATH_IMGUI_INTEGRATION` | `imgui` | `OFF` | Adds the `to_im_vec2`/`to_im_vec4`/`to_im_color` conversions and the `omath::hud::ImguiHudRenderer`. Links ImGui publicly. |
| `OMATH_ENABLE_HOOKING` | `hooking` | `OFF` | Compiles `omath::hooks::HooksManager` (DirectX 9/11/12 and OpenGL hooks) on top of safetyhook. Windows and Linux only, not ARM. Without it the class has no hook methods. |
| `OMATH_ENABLE_LUA` | `lua` | `OFF` | Compiles the Lua bindings (`omath::lua::LuaInterpreter`) on top of Lua and sol2. |
| `OMATH_ENABLE_MODULES` | none | `OFF` | Builds the C++ module interface (`import omath;`). Needs CMake 3.28 or newer and a compiler with module support. |
| `OMATH_BUILD_AS_SHARED_LIBRARY` | none | `OFF` | Build a `.so`/`.dll` instead of a static library. |
| `OMATH_STATIC_MSVC_RUNTIME_LIBRARY` | none | `OFF` | Link the static MSVC runtime. |
| `OMATH_THREAT_WARNING_AS_ERROR` | none | `ON` | Highest warning level, warnings are errors (applies to the library's own sources). |
| `OMATH_SUPRESS_SAFETY_CHECKS` | none | `ON` | Skips some safety checks in release builds. Note the spelling of the option. |
| `OMATH_ENABLE_FORCE_INLINE` | none | `ON` | Forces inlining of `EncryptedVariable` helpers. |
| `OMATH_ENABLE_LEGACY` | none | `ON` | Defines `OMATH_ENABLE_LEGACY` for classes kept for backward compatibility. The macro is exported, but no header is currently guarded by it, so turning it off changes nothing yet. |
| `OMATH_ENABLE_COVERAGE` | none | `OFF` | Coverage instrumentation, only together with `OMATH_BUILD_TESTS`. |

The vcpkg feature `all` enables `imgui`, `lua` and `hooking` together. The `examples` feature pulls in GLFW, GLEW and ImGui bindings, and on Windows also the hooking feature.

Configure an example build with tests and the ImGui integration:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DOMATH_BUILD_TESTS=ON -DOMATH_IMGUI_INTEGRATION=ON
cmake --build build --target unit_tests omath
```

When a feature adds a third-party dependency (ImGui, Lua, safetyhook) a downstream `find_package(omath)` looks for it again, so make it discoverable in the consuming project as well (same vcpkg toolchain file, or `CMAKE_PREFIX_PATH`).

## Verify the installation

```cpp
#include <omath/omath.hpp>
#include <iostream>

int main()
{
    const omath::Vector3<float> v{3.f, 4.f, 0.f};
    std::cout << v.length() << "\n"; // 5
}
```

If this compiles and prints `5`, headers and library are wired up. Continue with [Getting Started](getting_started.md).
