---
title: OMath
hide:
  - navigation
  - toc
---

<div class="hero" markdown>

<img class="hero-logo" src="images/logos/omath_logo_macro.png" alt="OMath logo">

# OMath { .hero-title }

<p class="hero-tagline">
A 100% independent, <code>constexpr</code>, template-based math, physics and game-tooling framework for modern C++23 — no legacy code, no dependencies.
</p>

[Get started](getting_started.md){ .md-button .md-button--primary }
[Install](install.md){ .md-button }
[API overview](api_overview.md){ .md-button }

<p>
  <a href="https://git.libomath.org/orange/omath/src/branch/main/LICENSE"><img src="https://img.shields.io/github/license/alt-orange/omath" alt="License"></a>
  <img src="https://img.shields.io/gitea/v/release/orange/omath?gitea_url=https://git.libomath.org&sort=semver" alt="Release">
  <img src="https://img.shields.io/gitea/last-commit/orange/omath?gitea_url=https://git.libomath.org" alt="Last commit">
  <img src="https://img.shields.io/github/actions/workflow/status/alt-orange/omath/cmake-multi-platform.yml" alt="Build status">
  <a href="https://repology.org/project/orange-math/versions"><img src="https://repology.org/badge/version-for-repo/vcpkg/orange-math.svg" alt="Vcpkg package"></a>
  <img src="https://img.shields.io/conan/v/omath" alt="Conan Center">
  <a href="https://discord.gg/eDgdaWbqwZ"><img src="https://dcbadge.limes.pink/api/server/https://discord.gg/eDgdaWbqwZ?style=flat" alt="Join us on Discord"></a>
  <a href="https://t.me/orangennotes"><img src="https://img.shields.io/badge/Telegram-2CA5E0?style=flat-square&logo=telegram&logoColor=white" alt="Telegram"></a>
</p>

</div>

## Quick example

```cpp
#include <omath/omath.hpp>

using omath::Vector3;

Vector3<float> a{1, 2, 3};
Vector3<float> b{4, 5, 6};

auto dot      = a.dot(b);          // 32.0
auto cross    = a.cross(b);        // (-3, 6, -3)
auto distance = a.distance_to(b);  // ~5.196
auto unit     = a.normalized();    // length == 1

// World-to-screen projection (Source Engine)
using namespace omath::source_engine;
Camera camera(position, angles, viewport, fov, near_plane, far_plane);

if (auto screen = camera.world_to_screen(world_position)) {
    // draw at screen->x, screen->y
}
```

## What is inside

<div class="grid cards" markdown>

-   :material-vector-triangle: **Linear algebra & geometry**

    ---

    Vectors, matrices, quaternions, strongly-typed angles, triangles, planes, boxes, AABB and OBB.

    [Vector3](linear_algebra/vector3.md) · [Mat4X4](linear_algebra/mat.md) · [Quaternion](linear_algebra/quaternion.md) · [Angle](trigonometry/angle.md)

-   :material-camera-outline: **Projection & cameras**

    ---

    Build your own projection pipeline and project world positions to the screen without hunting for the view-projection matrix.

    [Camera](projection/camera.md) · [Error codes](projection/error_codes.md)

-   :material-cube-scan: **Collision**

    ---

    Line tracing, mesh colliders, GJK and EPA behind simple interfaces.

    [Line tracer](collision/line_tracer.md) · [GJK](collision/gjk_algorithm.md) · [EPA](collision/epa_algorithm.md)

-   :material-bullseye-arrow: **Projectile prediction**

    ---

    O(N) aim-point solvers: legacy, AVX2 and a drag-aware engine for rounds that lose speed in flight.

    [Engine interface](projectile_prediction/projectile_engine.md) · [Drag engine](projectile_prediction/proj_pred_engine_drag.md)

-   :material-controller: **Engine support**

    ---

    Coordinate systems for Source, Unity, Unreal, Frostbite, IW, CryEngine, RAGE, Serious Engine and canonical OpenGL.

    [Browse engines](#engine-support)

-   :material-layers-outline: **HUD & overlays**

    ---

    Compose ESP overlays declaratively with widgets, gradients and an ImGui renderer.

    [HUD overview](hud/overview.md) · [Entity overlay](hud/entity_overlay.md)

-   :material-hook: **Hooking**

    ---

    `HooksManager` hooks DirectX 9/11/12 and OpenGL presents for you, so you only write the draw code.

    [HooksManager](hooks/hooks_manager.md)

-   :material-language-lua: **Lua scripting**

    ---

    Bindings for vectors, matrices, engines, collision, colors and HUD, available out of the box.

    [Lua bindings](lua/lua.md)

-   :material-memory: **Reverse engineering**

    ---

    Typed internal/external memory access and byte-pattern scanning in PE, ELF and Mach-O images.

    [External objects](rev_eng/external_rev_object.md) · [Pattern scan](utility/pattern_scan.md)

-   :material-map-marker-path: **Pathfinding**

    ---

    A\*, triangle navigation meshes and a walk bot.

    [A\*](pathfinding/a_star.md) · [Navigation mesh](pathfinding/navigation_mesh.md) · [Walk bot](pathfinding/walk_bot.md)

</div>

## Common use cases

| Use case | What you get | Start with |
|----------|--------------|------------|
| World-to-screen | Project 3D points to 2D for ESP, UI and visualization | [Camera](projection/camera.md) |
| Projectile prediction | Aim points for moving targets with speed, gravity and drag | [Projectile engines](projectile_prediction/projectile_engine.md) |
| Collision detection | Ray casts, line traces and intersection tests | [Line tracer](collision/line_tracer.md) |
| Pattern scanning | Byte patterns with wildcards across PE, ELF and Mach-O | [Pattern scan](utility/pattern_scan.md) |
| Overlays | Declarative HUD widgets drawn over a hooked render loop | [HUD overview](hud/overview.md) |
| Pathfinding | Routes through 3D spaces with A\* and nav meshes | [A\*](pathfinding/a_star.md) |

## Engine support

Every engine ships a camera trait, projectile-prediction trait, mesh trait, constants and formulas, so the same code works with each game's coordinate system.

| Engine | Typical games | Pages |
|--------|---------------|-------|
| Source | Half-Life 2, TF2, CS:GO | [Camera](engines/source_engine/camera_trait.md) · [Constants](engines/source_engine/constants.md) · [Formulas](engines/source_engine/formulas.md) |
| Unity | Unity titles | [Camera](engines/unity_engine/camera_trait.md) · [Constants](engines/unity_engine/constants.md) · [Formulas](engines/unity_engine/formulas.md) |
| Unreal | Unreal titles, UE 2.5 – UE 5 | [Camera](engines/unreal_engine/camera_trait.md) · [Actor name](engines/unreal_engine/actor_name.md) · [Object array](engines/unreal_engine/object_array.md) |
| Frostbite | Battlefield and other EA titles | [Camera](engines/frostbite/camera_trait.md) · [Constants](engines/frostbite/constants.md) · [Formulas](engines/frostbite/formulas.md) |
| IW | Call of Duty | [Camera](engines/iw_engine/camera_trait.md) · [Constants](engines/iw_engine/constants.md) · [Formulas](engines/iw_engine/formulas.md) |
| CryEngine | Crytek titles | [Camera](engines/cry_engine/camera_trait.md) · [Constants](engines/cry_engine/constants.md) · [Formulas](engines/cry_engine/formulas.md) |
| RAGE | Rockstar titles | [Camera](engines/rage_engine/camera_trait.md) · [Constants](engines/rage_engine/constants.md) · [Formulas](engines/rage_engine/formulas.md) |
| Serious Engine | Serious Sam | [Camera](engines/serious_engine/camera_trait.md) · [Constants](engines/serious_engine/constants.md) · [Formulas](engines/serious_engine/formulas.md) |
| OpenGL | Canonical OpenGL right-handed space | [Camera](engines/opengl_engine/camera_trait.md) · [Constants](engines/opengl_engine/constants.md) · [Formulas](engines/opengl_engine/formulas.md) |

## Key features

- **Efficiency**: optimized for performance, with AVX2 where available.
- **Versatility**: a wide array of mathematical functions and algorithms.
- **Ease of use**: a simple interface that is easy to drop into existing projects.
- **Projectile prediction**: O(N) engines that can power a projectile aim-bot.
- **3D projection**: build your own projection pipeline, no view-projection matrix hunting.
- **Collision detection**: production-ready code behind simple interfaces.
- **No additional dependencies**: nothing is needed except for unit tests, and the optional ImGui, Lua and hooking features.
- **Meta-programming ready**: templates for vectors, matrices and more, so every numeric type works.
- **Cross platform**: Windows, macOS and Linux.
- **Reverse engineering**: typed offset access, pattern scanning in PE/ELF/Mach-O (even Wine apps), Unreal name and object-table resolution.
- **Hooking & HUD**: DirectX 9/11/12 and OpenGL hooks plus a declarative overlay toolkit.
- **Scripting**: Lua bindings out of the box.

## Gallery

[![Youtube Video](images/yt_previews/img.png)](https://youtu.be/lM_NJ1yCunw?si=-Qf5yzDcWbaxAXGQ)

![APEX Preview]

![BO2 Preview]

![CS2 Preview]

![TF2 Preview]

![OpenGL Preview]

## Community & support

- **Documentation**: [libomath.org](http://libomath.org)
- **Source**: [git.libomath.org/orange/omath](https://git.libomath.org/orange/omath)
- **Discord**: [Join our community](https://discord.gg/eDgdaWbqwZ)
- **Telegram**: [@orangennotes](https://t.me/orangennotes)
- **Contributing**: see [CONTRIBUTING.md](https://git.libomath.org/orange/omath/src/branch/main/CONTRIBUTING.md)

<!----------------------------------{ Images }--------------------------------->
[APEX Preview]: images/showcase/apex.png
[BO2 Preview]: images/showcase/cod_bo2.png
[CS2 Preview]: images/showcase/cs2.jpeg
[TF2 Preview]: images/showcase/tf2.jpg
[OpenGL Preview]: images/showcase/opengl.png
