//
// Created by Vlad on 8/14/2026.
//
// Killing Floor 1 (KF_VERSION 1065, Unreal Engine 2.5) overlay as a DLL. Where example_kf1_overlay watches the game
// from a second process and draws into a window of its own, this one lives inside the game: omath's HooksManager
// hooks IDirect3DDevice9::Present and ImGui draws straight onto the game's back buffer, so there is no window to keep
// aligned and nothing to composite.
//
// The game has to run its Direct3D 9 renderer (D3D9Drv). Build it 32 bit, KillingFloor.exe is a 32 bit process.
//
// The game's memory is read with the same kf1_common/kf1_target.hpp the standalone overlay uses. ReadProcessMemory
// on the current process is kept on purpose instead of plain pointer reads: it reports a bad address as a failed read
// instead of faulting inside the game. Present runs on the game's own thread, so no read ever races a game tick.
//
// END unloads the DLL, and only counts while the game window is in front. There is nothing to click in the overlay,
// so the game's window procedure and its input are left alone.
//
// INSERT opens nullcore's desktop over the game: a task bar with a start button, whose menu opens the Settings window
// (an Aim-Bot and an ESP tab) and the Sensors window, which sets up nullcore's radar widgets. Those stay on screen with
// the desktop closed and are dragged into place with it open. Holding the aim key (the right mouse button
// by default) pulls
// the view onto the head of the zed closest to the crosshair, as long as nothing solid is in the way. That is the one
// thing in here that writes to the game: the player controller's rotation. With silent aim on, the rotation only
// points at the head for the game's tick, which is when shots are fired, and is put back before the frame is drawn, so
// the shots land on the head while the camera stays where the player left it.
//
// The ESP tab's glow goes on while the level is drawn, not at Present: the engine's calls are hooked to tell the draws
// of a zed, an ally or an ally's weapon from the rest, see kf1_actor_glow.hpp. It only reaches what the engine renders
// at all, so a zed that a level's zones cull stays without it.
//
// While the menu is open the game's window procedure is hooked, so the menu gets the mouse and keyboard and the game
// does not see what the menu uses. The game may be holding the mouse itself, in which case the cursor cannot be moved:
// opening the game's own menu with Escape gives it back, and the menu can be driven with the arrow keys and Enter.

// Has to come first: it defines WIN32_LEAN_AND_MEAN before anything pulls in Windows.h, the DirectX headers it
// includes do not parse after a plain Windows.h
#include "omath/hooks/hooks_manager.hpp"
// clang-format off
#include "kf1_common/kf1_target.hpp"
// clang-format on
#include "kf1_overlay_common.hpp"
#include "kf1_actor_glow.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <d3d9.h>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <ranges>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <imgui_nullcore/imgui_nullcore.hpp>
#include <psapi.h>
#include <thread>
#include <tuple>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace nc = imgui_nullcore;

namespace
{
// The desktop's icons, nullcore's own as imgui-nullcore's preview carries them
#include "images.inc"

    // Resolving the reflection classes walks every object name, so a failed attempt is retried once a second at most
    constexpr auto k_retry_interval = std::chrono::seconds(1);

    // How long a Present that is already inside the detour gets to leave it once the hook is removed, before the DLL
    // is unmapped under it
    constexpr DWORD k_unhook_grace_ms = 500;

    constexpr auto k_memory_note_interval = std::chrono::minutes(1);

    constexpr auto k_settings_window = "Settings";
    constexpr auto k_sensors_window = "Sensors";

    // Where the sensors sit until they are dragged somewhere else, counted from the top right corner of the screen
    constexpr ImVec2 k_sensors_corner_offset{260.f, 180.f};

    struct AimKey final
    {
        const char* name;
        int code;
    };

    constexpr std::array k_aim_keys{AimKey{"Right mouse", VK_RBUTTON}, AimKey{"Left mouse", VK_LBUTTON},
                                    AimKey{"Mouse 4", VK_XBUTTON1},    AimKey{"Mouse 5", VK_XBUTTON2},
                                    AimKey{"Left Shift", VK_LSHIFT},   AimKey{"Left Alt", VK_LMENU}};

    // What the Aim-Bot tab changes
    struct AimSettings final
    {
        bool enabled = true;
        // Index into k_aim_keys, the key that is held to aim. The right mouse button is the iron sights, which is when
        // a player wants the view on a head anyway.
        int key = 0;
        // Only zeds this close to where the view already points are considered, and only within the range
        float cone_degrees = 25.f;
        float range = 4000.f;
        // The share of the remaining angle covered per frame, 1 would snap
        float smoothing = .35f;
        // The collision cylinder has no head, so it is taken to sit this share of the height below the top of it
        float head_drop = .1f;
        bool visible_only = true;
        // Aim for the game's tick only, the camera keeps pointing where the player looks
        bool silent = false;
        bool no_recoil = false;
        bool no_spread = false;
    };

    // What the ESP tab's glow changes
    struct GlowSettings final
    {
        bool enabled = true;
        ImVec4 color{.31f, .86f, .47f, 1.f};
        // The allies' glow, in their ESP colour
        bool allies = true;
        bool ally_weapons = true;
        ImVec4 ally_weapon_color{1.f, 1.f, 1.f, 1.f};
        // The blur's scale, how far out the glow stays bright
        float thickness = 5.f;
    };

    using RawRotation = std::array<std::int32_t, 3>;

    // The controller rotation around a silent aim, as the raw pitch, yaw and roll units the engine stores. The tick
    // copies it onto the pawn and the weapon too, so those are put back along with the controller.
    struct SilentAim final
    {
        std::uintptr_t controller{};
        RawRotation real;
        RawRotation written;
        std::uintptr_t pawn{};
        RawRotation pawn_real;
        std::uintptr_t weapon{};
        RawRotation weapon_real;
    };

    // A texture of ImGui's, gone with the context like the rest of them, see on_reset
    struct Icon final
    {
        const unsigned char* rgba;
        int size;
        ImTextureData* texture{};
    };

    Icon g_start_icon{k_image_start_button, 16};
    Icon g_settings_icon{k_image_icon_settings, 16};
    Icon g_sensors_icon{k_image_icon_maze, 16};
    Icon g_logo{k_image_logo_nullcore, 64};

    // Where the sensors find what they show on the player's pawn, resolved by name for the pawn's class
    struct PawnLayout final
    {
        std::uintptr_t pawn_class{};
        kf1_overlay::ZedLayout health;
        std::optional<std::int32_t> velocity;
    };

    HMODULE g_module{};
    HWND g_game_window{};
    bool g_imgui_ready = false;
    // The device ImGui draws with
    IDirect3DDevice9* g_imgui_device{};

    std::optional<kf1::Target> g_target;
    kf1::ReflectionClasses g_classes;
    kf1::LocalPlayer g_player;
    std::chrono::steady_clock::time_point g_last_attempt;
    std::chrono::steady_clock::time_point g_last_player_search;

    kf1_overlay::ActorTracker g_tracker;
    // The sensors bring their own radar
    kf1_overlay::EspSettings g_esp{.radar = false};
    bool g_sensors_enabled = true;
    // The crit gauge shows the player's health. The Sequence Burst bar stays off, KF1 has nothing to put on it that its
    // fixed "SBT" readout would describe.
    nc::SensorsConfig g_sensors_config{.Zoom = 10.f, .BurstIndicator = false};
    // Kept out here because nc's own copy goes with the ImGui context
    nc::SensorsLayout g_sensors_layout;
    PawnLayout g_pawn_layout;
    AimSettings g_aim;
    std::optional<SilentAim> g_silent;
    // The zeds the last overlay frame read, aimed at once the engine is done drawing
    std::vector<kf1_overlay::LiveZed> g_aim_zeds;
    // The allies of the last overlay frame, kept apart from the above so the aim never sees one
    std::vector<kf1_overlay::LiveZed> g_allies;
    std::vector<std::uintptr_t> g_ally_weapons;
    safetyhook::InlineHook g_draw_hook;
    GlowSettings g_glow;
    kf1_overlay::ActorGlow g_zed_glow;
    kf1_overlay::ActorGlow g_ally_glow;
    kf1_overlay::ActorGlow g_ally_weapon_glow;
    safetyhook::InlineHook g_level_render_hook;
    safetyhook::InlineHook g_mesh_render_hook;
    safetyhook::InlineHook g_draw_indexed_primitive_hook;
    safetyhook::InlineHook g_draw_primitive_hook;
    safetyhook::MidHook g_create_device_hook;
    // How deep inside FLevelSceneNode::Render the game is, a mirror or a sky zone renders a level inside the level.
    // Outside of it the game draws the first person weapon and the menus.
    int g_level_render_depth = 0;
    // The glow of the mesh the game is drawing, null for one that has none
    kf1_overlay::ActorGlow* g_mesh_glow = nullptr;
    // The game is drawing a mesh over the level with the glows still to come: the first person weapon
    bool g_mesh_cutout = false;
    // The level is done and the HUD's overlay has not been drawn yet
    bool g_overlay_watch = false;
    // Whether the last frame had that overlay, which is when a glow has to wait for it
    bool g_hud_overlay = false;
    // The glows are blurred and waiting for the overlay
    bool g_glows_waiting = false;

    void restore_view();
    void restore_recoil();
    void restore_spread();
    [[nodiscard]]
    std::optional<ImVec2> back_buffer_size(IDirect3DDevice9* device);
    // Pawn::Weapon, resolved by name when a silent aim starts
    std::ptrdiff_t g_weapon_offset = 0;
    bool g_menu_open = false;
    // The desktop's state is the ImGui context's, so a new context starts with its windows closed
    bool g_desktop_ready = false;
    bool g_insert_was_down = false;
    bool g_end_was_down = false;
    std::atomic<bool> g_unload_requested{false};
    // Set by the game thread once ImGui is gone, which is when the unload may go on
    std::atomic<bool> g_imgui_shut_down{false};

    // A fault inside the overlay must not take the game with it. The first one is written to a report next to the DLL,
    // then the overlay stays off for good.
    bool g_faulted = false;
    // Which part of the frame is running, so a report says where it went wrong
    const char* g_stage = "idle";
    // The first few frames write every stage to the log before running it, so even a crash nothing can catch leaves
    // the last stage reached behind
    constexpr int k_traced_frames = 8;
    int g_frame = 0;

    // Milestones go to a log next to the DLL, a crash then still tells how far the overlay got. Only called for events
    // that happen once or a handful of times, never per frame.
    void note(const std::string& text)
    {
        wchar_t module_path[MAX_PATH]{};
        GetModuleFileNameW(g_module, module_path, MAX_PATH);

        std::ofstream(std::filesystem::path(module_path).replace_filename(L"kf1_overlay.log"), std::ios::app)
                << std::format("[{:>8} ms] {}\n", GetTickCount64() % 100000000, text);
    }

    // How much of the 4 GB the game has left. It ends with "Ran out of virtual memory" when they are gone, and these
    // lines tell what spends them: time, a level or a new device.
    void note_memory()
    {
        constexpr auto k_megabyte = 1024ull * 1024ull;

        MEMORYSTATUSEX status{.dwLength = sizeof(MEMORYSTATUSEX)};
        PROCESS_MEMORY_COUNTERS counters{.cb = sizeof(PROCESS_MEMORY_COUNTERS)};

        if (GlobalMemoryStatusEx(&status) && GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
            note(std::format("memory: {} of {} MB of address space in use, {} MB committed, {} objects",
                             (status.ullTotalVirtual - status.ullAvailVirtual) / k_megabyte,
                             status.ullTotalVirtual / k_megabyte, counters.PagefileUsage / k_megabyte,
                             g_target.has_value() ? kf1::live_object_count(*g_target) : 0));
    }

    // The same lookup attach() does for another process, minus the OpenProcess and the console output: a game has no
    // stdout to print to. The current process pseudo handle is all RpmTrait needs.
    [[nodiscard]]
    std::optional<kf1::Target> attach_in_process()
    {
        kf1::g_process = GetCurrentProcess();

        const auto core_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"Core.dll"));
        const auto engine_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"Engine.dll"));

        if (!core_base || !engine_base)
            return std::nullopt;

        kf1::Target target{.process_id = GetCurrentProcessId(),
                           .engine_base = engine_base,
                           .g_obj_objects = core_base + kf1::k_g_obj_objects_rva,
                           .g_names = core_base + kf1::k_g_names_rva,
                           .object_array = kf1::object_array_layout(),
                           .names = kf1::name_layout()};

        const auto count = kf1::get_object_count<kf1::RpmTrait>(target.g_obj_objects, target.object_array);

        if (!count.has_value())
            return std::nullopt;

        target.object_count = *count;

        return target;
    }

    // Keeps trying until the engine has its Actor/PlayerController/Pawn/Property classes registered, which is not the
    // case yet while the game is still starting up
    [[nodiscard]]
    bool game_ready()
    {
        if (g_classes.complete())
            return true;

        const auto now = std::chrono::steady_clock::now();

        if (now - g_last_attempt < k_retry_interval)
            return false;

        g_last_attempt = now;
        g_target = attach_in_process();

        // The attempts repeat every second until the engine is up, only the first few are worth a line each
        static auto attempts = 0;
        const auto report = attempts++ < 5;

        if (!g_target.has_value())
        {
            if (report)
                note("attach: Core.dll/Engine.dll/GObjObjects not readable yet");

            return false;
        }

        g_classes = kf1::find_reflection_classes(*g_target);

        if (report || g_classes.complete())
            note(std::format("reflection classes complete={} (actor {:X}, player controller {:X}, pawn {:X}, "
                             "property {:X}), {} objects",
                             g_classes.complete(), g_classes.actor, g_classes.player_controller, g_classes.pawn,
                             g_classes.property, g_target->object_count));

        if (!g_classes.complete())
            return false;

        g_player = kf1::find_local_player(*g_target, g_classes);
        note(std::format("local player: controller {:X}, pawn {:X}, fov offset {}", g_player.controller, g_player.pawn,
                         g_player.fov_offset.value_or(-1)));

        return true;
    }

    // on_present tears ImGui down on the game's thread, so nothing is drawing while it goes. This thread then removes
    // the Present hook, gives a Present that is already inside the detour time to leave it, and only then lets go of
    // the DLL. DllMain is no place for any of that, the loader lock is held there.
    DWORD WINAPI unload_thread(LPVOID)
    {
        while (!g_imgui_shut_down)
            Sleep(10);

        restore_view();
        restore_recoil();
        restore_spread();
        g_draw_hook = {};
        g_level_render_hook = {};
        g_mesh_render_hook = {};
        g_draw_indexed_primitive_hook = {};
        g_draw_primitive_hook = {};
        g_create_device_hook = {};
        omath::hooks::HooksManager::get().unhook_dx9();
        Sleep(k_unhook_grace_ms);
        FreeLibraryAndExitThread(g_module, 0);
    }

    void request_unload()
    {
        g_unload_requested = true;

        if (const auto thread = CreateThread(nullptr, 0, unload_thread, nullptr, 0, nullptr))
            CloseHandle(thread);
    }

    // hFocusWindow is null for a device created without one, the swap chain still knows the window it presents into
    [[nodiscard]]
    HWND find_device_window(IDirect3DDevice9* device)
    {
        D3DDEVICE_CREATION_PARAMETERS params{};

        if (SUCCEEDED(device->GetCreationParameters(&params)) && params.hFocusWindow)
            return params.hFocusWindow;

        IDirect3DSwapChain9* swap_chain{};

        if (FAILED(device->GetSwapChain(0, &swap_chain)))
            return nullptr;

        D3DPRESENT_PARAMETERS present_parameters{};
        const auto window = SUCCEEDED(swap_chain->GetPresentParameters(&present_parameters))
                                    ? present_parameters.hDeviceWindow
                                    : nullptr;
        swap_chain->Release();

        return window;
    }

    void set_stage(const char* stage)
    {
        g_stage = stage;

        if (g_frame <= k_traced_frames)
            note(std::format("frame {}: {}", g_frame, stage));
    }

    [[nodiscard]]
    omath::Vector3<double> head_position(const kf1_overlay::ZedBounds& bounds)
    {
        auto head = bounds.top();
        head.z -= (bounds.max.z - bounds.min.z) * g_aim.head_drop;

        return head;
    }

    // Where AActor::execGetBoneCoords finds the mesh, and the two virtuals it goes through
    constexpr std::ptrdiff_t k_actor_mesh_offset = 0xB8; // UMesh*
    constexpr std::ptrdiff_t k_actor_is_pending_kill_slot = 0x30; // AActor::IsPendingKill()
    constexpr std::ptrdiff_t k_mesh_get_instance_slot = 0x98; // UMesh::MeshGetInstance(const AActor*)
    constexpr std::ptrdiff_t k_mesh_instance_match_ref_bone_slot = 0x130; // UMeshInstance::MatchRefBone(FName)

    // Whether an address lies inside a loaded module's image
    [[nodiscard]]
    bool in_module(const std::uintptr_t address)
    {
        HMODULE module{};

        return address
               && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                             | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     reinterpret_cast<LPCSTR>(address), &module);
    }

    // An object's virtual function, 0 unless both its vtable and the function sit in a module, Engine.dll or the game
    // module whose native class it is. Anything else is an object that has been freed and its memory taken for
    // something else, on the heap: calling through it would jump into data.
    [[nodiscard]]
    std::uintptr_t module_virtual(const std::uintptr_t object, const std::ptrdiff_t slot)
    {
        const auto vtable = kf1::read_ptr(object);
        const auto function = in_module(vtable) ? kf1::read_ptr(vtable, slot) : 0;

        return in_module(function) ? function : 0;
    }

    // KFMonster::HeadBone, HeadHeight and HeadScale, declared on KFMonster, so the same for every zed
    struct HeadOffsets final
    {
        std::int32_t head_bone{};
        std::int32_t head_height{};
        std::int32_t head_scale{};
    };

    std::optional<HeadOffsets> g_head_offsets;
    bool g_head_offsets_resolved = false;

    // The point KFMonster::IsHeadShot measures a hit against: the head bone's origin, moved HeadHeight * HeadScale
    // along the bone's X axis. The bone comes from the same calls GetBoneCoords makes in script, so it follows the
    // animation. Nothing when the zed has no skeletal mesh or no head bone.
    [[nodiscard]]
    std::optional<omath::Vector3<double>> head_bone_position(const std::uintptr_t actor)
    {
        using GetBoneCoords = void*(__thiscall*)(void* instance, float* coords, std::uint32_t bone);
        using MeshGetInstance = void*(__thiscall*)(void* mesh, const void* actor);
        using MatchRefBone = int(__thiscall*)(void* instance, std::int32_t bone_name);
        using IsPendingKill = int(__thiscall*)(void* actor);

        static const auto get_bone_coords = reinterpret_cast<GetBoneCoords>(GetProcAddress(
                GetModuleHandleW(L"Engine.dll"), "?GetBoneCoords@USkeletalMeshInstance@@QAE?AVFCoords@@K@Z"));

        if (!g_head_offsets_resolved)
        {
            g_head_offsets_resolved = true;

            const auto actor_class = kf1::read_ptr(actor, kf1::k_object_class_offset);
            const auto head_bone = kf1::find_property_offset(*g_target, actor_class, "HeadBone", g_classes.property);
            const auto head_height = kf1::find_property_offset(*g_target, actor_class, "HeadHeight", g_classes.property);
            const auto head_scale = kf1::find_property_offset(*g_target, actor_class, "HeadScale", g_classes.property);

            if (head_bone.has_value() && head_height.has_value() && head_scale.has_value())
                g_head_offsets = HeadOffsets{*head_bone, *head_height, *head_scale};

            note(std::format("head bones {}", g_head_offsets.has_value() && get_bone_coords ? "ok" : "unavailable"));
        }

        if (!g_head_offsets.has_value() || !get_bone_coords)
            return std::nullopt;

        // A zed on its way out has its mesh instance torn down already, the bone update then reads freed memory.
        // execGetBoneCoords asks the same before it goes near the bones.
        const auto is_pending_kill =
                reinterpret_cast<IsPendingKill>(module_virtual(actor, k_actor_is_pending_kill_slot));

        if (!is_pending_kill || is_pending_kill(reinterpret_cast<void*>(actor)))
            return std::nullopt;

        const auto mesh = kf1::read_ptr(actor, k_actor_mesh_offset);

        if (!mesh || kf1::object_name(*g_target, kf1::read_ptr(mesh, kf1::k_object_class_offset)) != "SkeletalMesh")
            return std::nullopt;

        const auto get_instance = reinterpret_cast<MeshGetInstance>(module_virtual(mesh, k_mesh_get_instance_slot));

        if (!get_instance)
            return std::nullopt;

        const auto instance = get_instance(reinterpret_cast<void*>(mesh), reinterpret_cast<void*>(actor));

        if (!instance)
            return std::nullopt;

        const auto match_ref_bone = reinterpret_cast<MatchRefBone>(
                module_virtual(reinterpret_cast<std::uintptr_t>(instance), k_mesh_instance_match_ref_bone_slot));

        if (!match_ref_bone)
            return std::nullopt;

        const auto bone = match_ref_bone(instance, kf1::read<std::int32_t>(actor + g_head_offsets->head_bone));

        if (bone < 0)
            return std::nullopt;

        // FCoords: Origin, then the X, Y and Z axes
        std::array<float, 12> coords{};
        get_bone_coords(instance, coords.data(), static_cast<std::uint32_t>(bone));

        const auto along = kf1::read<float>(actor + g_head_offsets->head_height)
                           * kf1::read<float>(actor + g_head_offsets->head_scale);

        return omath::Vector3<double>{coords[0] + coords[3] * along, coords[1] + coords[4] * along,
                                      coords[2] + coords[5] * along};
    }

    // Asks the engine's own line trace whether level geometry or a mover sits between two points, the way
    // AController::LineOfSightTo does: nothing hit, or the target itself hit, means it can be seen. Failing to reach the
    // trace counts as blocked, an aim that cannot be checked is better not made.
    [[nodiscard]]
    bool has_line_of_sight(const omath::Vector3<double>& from, const omath::Vector3<double>& to,
                           const std::uintptr_t target_actor)
    {
        // FCheckResult as the engine lays it out: Actor at +0x4, Time at +0x24 which starts at 1, Item at +0x28 which
        // starts at -1. It is a lot smaller than this buffer.
        constexpr std::size_t k_hit_actor = 0x4 / sizeof(std::uint32_t);
        constexpr std::size_t k_hit_time = 0x24 / sizeof(std::uint32_t);
        constexpr std::size_t k_hit_item = 0x28 / sizeof(std::uint32_t);

        using SingleLineCheck = int(__thiscall*)(void* level, void* hit, void* source_actor, const float* end,
                                                 const float* start, DWORD trace_flags, float extent_x, float extent_y,
                                                 float extent_z);

        const auto level = kf1::read_ptr(g_player.controller, kf1::k_actor_level_offset);
        const auto function = level ? kf1::read_ptr(kf1::read_ptr(level), kf1::k_level_single_line_check_offset) : 0;

        if (!function)
            return false;

        std::array<std::uint32_t, 32> hit{};
        hit[k_hit_time] = std::bit_cast<std::uint32_t>(1.f);
        hit[k_hit_item] = 0xFFFFFFFF;

        const std::array start{static_cast<float>(from.x), static_cast<float>(from.y), static_cast<float>(from.z)};
        const std::array end{static_cast<float>(to.x), static_cast<float>(to.y), static_cast<float>(to.z)};

        reinterpret_cast<SingleLineCheck>(function)(reinterpret_cast<void*>(level), hit.data(),
                                                    reinterpret_cast<void*>(g_player.controller), end.data(),
                                                    start.data(), kf1::k_trace_world_flags, 0.f, 0.f, 0.f);

        return hit[k_hit_actor] == 0 || hit[k_hit_actor] == target_actor;
    }

    // Puts the player's own rotation back after a silent aim. Whatever the engine did to the rotation in between, the
    // mouse input of that tick, is kept: it is added onto the real value.
    void restore_view()
    {
        if (!g_silent.has_value())
            return;


        const auto address = kf1::shift_address(g_silent->controller, kf1::k_actor_rotation_offset);
        const auto now = kf1::read<RawRotation>(address);
        RawRotation restored{};
        RawRotation moved{};

        for (auto axis = 0uz; axis < restored.size(); ++axis)
        {
            moved[axis] = now[axis] - g_silent->written[axis];
            restored[axis] = (g_silent->real[axis] + moved[axis]) & 0xFFFF;
        }

        std::ignore = kf1::write(address, restored);

        // A pawn only turns around its yaw, the weapon follows the whole view
        if (g_silent->pawn)
        {
            const auto pawn_address = kf1::shift_address(g_silent->pawn, kf1::k_actor_rotation_offset);
            auto pawn_rotation = kf1::read<RawRotation>(pawn_address);
            pawn_rotation[1] = (g_silent->pawn_real[1] + moved[1]) & 0xFFFF;
            std::ignore = kf1::write(pawn_address, pawn_rotation);
        }

        if (g_silent->weapon && kf1::read_ptr(g_silent->pawn, g_weapon_offset) == g_silent->weapon)
        {
            RawRotation weapon_rotation{};

            for (auto axis = 0uz; axis < weapon_rotation.size(); ++axis)
                weapon_rotation[axis] = (g_silent->weapon_real[axis] + moved[axis]) & 0xFFFF;

            std::ignore =
                    kf1::write(kf1::shift_address(g_silent->weapon, kf1::k_actor_rotation_offset), weapon_rotation);
        }

        g_silent.reset();
    }

    void aim_after_draw();

    // UGameEngine::Draw runs once per frame between the ticks that fire the weapons. On the way in the real rotation is
    // handed back for the camera and the first person weapon, on the way out the aim goes in for the next tick. Present
    // is no place for the latter: the renderer presents the previous frame from inside Draw, before the first person
    // weapon is drawn, and the bone lookups and line traces the aim makes are not meant to run in the middle of a
    // render either. __fastcall stands in for __thiscall: this arrives in ecx and edx goes unused, the rest are the
    // stack arguments the callee cleans up in both.
    void __fastcall on_engine_draw(void* self, void*, void* viewport, int flag, std::uint8_t* data, int* size)
    {
        restore_view();
        g_draw_hook.thiscall<void>(self, viewport, flag, data, size);
        aim_after_draw();
    }

    void hook_engine_draw()
    {
        const auto engine = GetModuleHandleW(L"Engine.dll");
        const auto draw = engine ? GetProcAddress(engine, "?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z") : nullptr;

        if (!draw)
        {
            note("UGameEngine::Draw not found, silent aim unavailable");
            return;
        }

        auto hook = safetyhook::InlineHook::create(draw, reinterpret_cast<void*>(on_engine_draw));

        if (hook)
            g_draw_hook = std::move(*hook);

        note(std::format("hook UGameEngine::Draw {}", hook ? "ok" : "FAILED"));
    }

    // Points the controller at a head for the tick that is about to run
    void aim_silently(const omath::unreal_engine::ViewAngles& wanted)
    {
        const auto address = kf1::shift_address(g_player.controller, kf1::k_actor_rotation_offset);
        const auto real = kf1::read<RawRotation>(address);
        const RawRotation written{kf1::degrees_to_rotator_units(wanted.pitch.as_degrees()),
                                  kf1::degrees_to_rotator_units(wanted.yaw.as_degrees()), real[2]};

        SilentAim silent{g_player.controller, real, written};

        if (g_player.pawn)
        {
            const auto weapon_offset = kf1::find_property_offset(
                    *g_target, kf1::read_ptr(g_player.pawn, kf1::k_object_class_offset), "Weapon", g_classes.property);

            silent.pawn = g_player.pawn;
            silent.pawn_real = kf1::read<RawRotation>(kf1::shift_address(g_player.pawn, kf1::k_actor_rotation_offset));

            if (weapon_offset.has_value())
            {
                g_weapon_offset = *weapon_offset;
                silent.weapon = kf1::read_ptr(g_player.pawn, g_weapon_offset);
                silent.weapon_real =
                        kf1::read<RawRotation>(kf1::shift_address(silent.weapon, kf1::k_actor_rotation_offset));
            }
        }

        if (kf1::write(address, written))
            g_silent = silent;
    }

    // Pulls the view towards the head of the zed nearest to the crosshair that can be seen, while the aim key is held.
    // The angles are written to the controller's Rotation, which the engine takes as the base for the next frame's
    // mouse input, so the player keeps steering on top of it.
    void aim_at_zeds(const std::vector<kf1_overlay::LiveZed>& zeds)
    {
        if (!g_aim.enabled || !(GetAsyncKeyState(k_aim_keys[g_aim.key].code) & 0x8000)
            || GetForegroundWindow() != g_game_window || !g_player.valid())
            return;

        const auto state = kf1::read_camera(g_player);
        const auto forward = omath::unreal_engine::forward_vector(state.angles);

        std::optional<omath::Vector3<double>> target;
        auto best_angle = static_cast<double>(g_aim.cone_degrees);

        for (const auto& zed : zeds)
        {
            if (!zed.bounds.has_value())
                continue;

            const auto head = head_bone_position(zed.actor->address).value_or(head_position(*zed.bounds));
            const auto to_head = head - state.position;
            const auto distance = to_head.length();

            if (distance <= 0. || distance > g_aim.range)
                continue;

            const auto angle = std::acos(std::clamp(forward.dot(to_head / distance), -1., 1.)) * 180. / std::numbers::pi;

            if (angle >= best_angle
                || (g_aim.visible_only && !has_line_of_sight(state.position, head, zed.actor->address)))
                continue;

            best_angle = angle;
            target = head;
        }

        if (!target.has_value())
            return;

        const auto wanted = omath::unreal_engine::CameraTrait::calc_look_at_angle(state.position, *target);

        if (g_aim.silent && g_draw_hook)
        {
            aim_silently(wanted);
            return;
        }

        const auto pitch = state.angles.pitch.as_degrees();
        const auto yaw = state.angles.yaw.as_degrees();

        // Yaw wraps, the short way round is the one to take
        const auto pitch_step = (wanted.pitch.as_degrees() - pitch) * g_aim.smoothing;
        const auto yaw_step = std::remainder(wanted.yaw.as_degrees() - yaw, 360.) * g_aim.smoothing;

        std::ignore = kf1::write_view_angles(g_player.controller, {kf1::PitchAngle::from_degrees(pitch + pitch_step),
                                                                    kf1::YawAngle::from_degrees(yaw + yaw_step),
                                                                    kf1::RollAngle::from_degrees(0.)});
    }

    // A game value that was zeroed, with what it was. The object it sits in has to still be there, its class where it
    // was, for the value to be put back.
    struct ZeroedValue final
    {
        std::uintptr_t object{};
        std::uintptr_t object_class{};
        std::uintptr_t address{};
        std::int32_t original{};
    };

    // The values one option zeroed, and the fire modes it already went through
    struct ZeroedFireValues final
    {
        std::vector<std::uintptr_t> fires;
        std::vector<ZeroedValue> values;
    };

    ZeroedFireValues g_zeroed_recoil;
    ZeroedFireValues g_zeroed_spread;
    // Weapon::FireMode, the two WeaponFire objects of a weapon
    std::optional<std::int32_t> g_fire_mode_offset;

    // The fire modes of the weapon in hand
    [[nodiscard]]
    std::vector<std::uintptr_t> held_fire_modes()
    {
        if (!g_player.pawn)
            return {};

        const auto weapon_offset = kf1::find_property_offset(
                *g_target, kf1::read_ptr(g_player.pawn, kf1::k_object_class_offset), "Weapon", g_classes.property);
        const auto weapon = weapon_offset.has_value() ? kf1::read_ptr(g_player.pawn, *weapon_offset) : 0;

        if (!weapon)
            return {};

        if (!g_fire_mode_offset.has_value())
            g_fire_mode_offset = kf1::find_property_offset(
                    *g_target, kf1::read_ptr(weapon, kf1::k_object_class_offset), "FireMode", g_classes.property);

        if (!g_fire_mode_offset.has_value())
            return {};

        std::vector<std::uintptr_t> fires;

        for (const auto mode : {0, 1})
            if (const auto fire = kf1::read_ptr(weapon, *g_fire_mode_offset + mode * 4))
                fires.push_back(fire);

        return fires;
    }

    // The class default object, what UnrealScript reads as Default.Property. UClass keeps it as an array of bytes laid
    // out like an instance, so the array whose data names the class itself as UObject::Class is the one.
    [[nodiscard]]
    std::uintptr_t class_defaults(const std::uintptr_t object_class)
    {
        for (std::ptrdiff_t offset = 0x30; offset < 0x200; offset += 4)
        {
            const auto data = kf1::read_ptr(object_class, offset);

            if (data && kf1::read<std::int32_t>(object_class + offset + 4) > kf1::k_object_class_offset
                && kf1::read_ptr(data, kf1::k_object_class_offset) == object_class)
                return data;
        }

        return 0;
    }

    void zero_value(ZeroedFireValues& zeroed, const std::uintptr_t object, const std::int32_t offset)
    {
        const auto address = object + offset;

        zeroed.values.push_back(
                {object, kf1::read_ptr(object, kf1::k_object_class_offset), address, kf1::read<std::int32_t>(address)});
        std::ignore = kf1::write(address, std::int32_t{0});
    }

    // Zeroes the named properties on every fire mode of the weapon in hand the first time it is seen, and on the fire
    // mode's class defaults too when the game reads them from there. An int and a float zero are the same bits.
    void zero_fire_values(ZeroedFireValues& zeroed, const std::initializer_list<std::string_view> names,
                          const bool with_defaults)
    {
        for (const auto fire : held_fire_modes())
        {
            if (std::ranges::contains(zeroed.fires, fire))
                continue;

            zeroed.fires.push_back(fire);

            const auto fire_class = kf1::read_ptr(fire, kf1::k_object_class_offset);
            const auto defaults = with_defaults ? class_defaults(fire_class) : 0;

            for (const auto name : names)
            {
                const auto offset = kf1::find_property_offset(*g_target, fire_class, name, g_classes.property);

                if (!offset.has_value())
                    continue;

                zero_value(zeroed, fire, *offset);

                if (defaults)
                    zero_value(zeroed, defaults, *offset);
            }
        }
    }

    // Newest first: a value zeroed twice, through two fire modes sharing class defaults, ends on its first original
    void restore_fire_values(ZeroedFireValues& zeroed)
    {
        for (const auto& value : zeroed.values | std::views::reverse)
            if (kf1::read_ptr(value.object, kf1::k_object_class_offset) == value.object_class)
                std::ignore = kf1::write(value.address, value.original);

        zeroed = {};
    }

    // KFFire::HandleRecoil rolls each shot's kick from these, so zero angles make for no recoil
    void remove_recoil()
    {
        zero_fire_values(g_zeroed_recoil, {"maxVerticalRecoilAngle", "maxHorizontalRecoilAngle"}, false);
    }

    void restore_recoil()
    {
        restore_fire_values(g_zeroed_recoil);
    }

    // KFFire::GetSpread starts from Default.Spread and grows towards MaxSpread while the trigger is held, which is
    // why the class defaults are zeroed along with the instance
    void remove_spread()
    {
        zero_fire_values(g_zeroed_spread, {"Spread", "MaxSpread"}, true);
    }

    void restore_spread()
    {
        restore_fire_values(g_zeroed_spread);
    }

    // "module.dll+0x1234" for an address inside a loaded module, the bare address otherwise
    [[nodiscard]]
    std::string describe_address(const std::uintptr_t address)
    {
        HMODULE module{};

        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCSTR>(address), &module))
            return std::format("0x{:08X}", address);

        char path[MAX_PATH]{};
        GetModuleFileNameA(module, path, MAX_PATH);

        return std::format("{}+0x{:X}", std::filesystem::path(path).filename().string(),
                           address - reinterpret_cast<std::uintptr_t>(module));
    }

    [[nodiscard]]
    std::uintptr_t read_word(const std::uintptr_t address)
    {
        std::uintptr_t value{};

        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), &value, sizeof(value), nullptr))
            return 0;

        return value;
    }

    void write_fault_report(const EXCEPTION_POINTERS& info)
    {
        const auto& record = *info.ExceptionRecord;

        auto report = std::format("stage {}, exception 0x{:08X} at {}", g_stage, record.ExceptionCode,
                                  describe_address(reinterpret_cast<std::uintptr_t>(record.ExceptionAddress)));

        if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
            report += std::format(", {} 0x{:08X}", record.ExceptionInformation[0] == 1 ? "write to" : "read of",
                                  record.ExceptionInformation[1]);

        // Walk the frame pointer chain for who called into it. Code built without frame pointers breaks the chain
        // early, what is found until then is still useful.
        auto frame = static_cast<std::uintptr_t>(info.ContextRecord->Ebp);

        for (auto depth = 0; frame && depth < 12; ++depth)
        {
            const auto return_address = read_word(frame + sizeof(std::uintptr_t));
            const auto next_frame = read_word(frame);

            if (!return_address || next_frame <= frame)
                break;

            report += std::format("\n    called from {}", describe_address(return_address));
            frame = next_frame;
        }

        wchar_t module_path[MAX_PATH]{};
        GetModuleFileNameW(g_module, module_path, MAX_PATH);

        std::ofstream(std::filesystem::path(module_path).replace_filename(L"kf1_overlay_fault.log"), std::ios::app)
                << report << "\n\n";
    }

    // Only hardware faults count. C++ exceptions and debugger messages pass through to whoever handles them.
    [[nodiscard]]
    LONG fault_filter(const EXCEPTION_POINTERS* info)
    {
        if ((info->ExceptionRecord->ExceptionCode & 0xF0000000) != 0xC0000000)
            return EXCEPTION_CONTINUE_SEARCH;

        write_fault_report(*info);

        return EXCEPTION_EXECUTE_HANDLER;
    }

    void hook_draw_indexed_primitive(IDirect3DDevice9* device);

    // Leaves ImGui untouched while no window is known, on_present simply tries again on the next frame
    void init_imgui(IDirect3DDevice9* device)
    {
        g_game_window = find_device_window(device);

        if (!g_game_window)
        {
            note("no window found for the device yet");
            return;
        }

        note(std::format("device {:X}, window {:X}, initialising ImGui", reinterpret_cast<std::uintptr_t>(device),
                         reinterpret_cast<std::uintptr_t>(g_game_window)));

        ImGui::CreateContext();
        // The menu is the only window and the game may hold the mouse, so it may never get the focus to be fully opaque
        nc::GetOptions().UnfocusedTransparency = false;
        // The ESP's labels are in Verdana bold
        g_esp.font = nc::LoadFonts(ImGui::GetIO()).Header;
        nc::StyleColorsNullcore();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().LogFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplWin32_Init(g_game_window);
        ImGui_ImplDX9_Init(device);

        // Only while the menu is open does anything reach ImGui. What the menu is using, the mouse over one of its
        // windows or the keyboard while it navigates, is kept from the game.
        auto& manager = omath::hooks::HooksManager::get();
        manager.set_on_wnd_proc(
                [](const HWND window, const UINT message, const WPARAM w_param,
                   const LPARAM l_param) -> std::optional<LRESULT>
                {
                    if (!g_imgui_ready || !g_menu_open)
                        return std::nullopt;

                    ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param);

                    const auto& io = ImGui::GetIO();
                    const auto is_mouse = message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
                    const auto is_key = message >= WM_KEYFIRST && message <= WM_KEYLAST;

                    if ((is_mouse && io.WantCaptureMouse) || (is_key && io.WantCaptureKeyboard))
                        return 0;

                    return std::nullopt;
                });
        std::ignore = manager.hook_wnd_proc(g_game_window);

        hook_draw_indexed_primitive(device);

        const auto size = back_buffer_size(device);

        if (g_sensors_layout.Center.x < 0.f && size.has_value())
            g_sensors_layout.Center = {size->x - k_sensors_corner_offset.x, k_sensors_corner_offset.y};

        nc::SetSensorsLayout(g_sensors_layout);

        g_imgui_device = device;
        g_imgui_ready = true;
        note("ImGui initialised");
        note_memory();
    }

    [[nodiscard]]
    ImTextureRef icon_texture(Icon& icon)
    {
        if (!icon.texture)
        {
            icon.texture = IM_NEW(ImTextureData)();
            icon.texture->Create(ImTextureFormat_RGBA32, icon.size, icon.size);
            std::memcpy(icon.texture->Pixels, icon.rgba, static_cast<std::size_t>(icon.size * icon.size * 4));
            icon.texture->UseColors = true;
            ImGui::RegisterUserTexture(icon.texture);
        }

        return icon.texture->GetTexRef();
    }

    // After the renderer's shutdown, which has dropped what the device held of them
    void free_icons()
    {
        for (auto* icon : {&g_start_icon, &g_settings_icon, &g_sensors_icon, &g_logo})
        {
            if (!icon->texture)
                continue;

            ImGui::UnregisterUserTexture(icon->texture);
            IM_DELETE(icon->texture);
            icon->texture = nullptr;
        }
    }

    void shutdown_imgui()
    {
        omath::hooks::HooksManager::get().unhook_wnd_proc();
        g_imgui_ready = false;
        g_desktop_ready = false;
        g_sensors_layout = nc::GetSensorsLayout();
        g_zed_glow.release();
        g_ally_glow.release();
        g_ally_weapon_glow.release();

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        free_icons();
        nc::Shutdown();
        ImGui::DestroyContext();
    }

    void draw_aim_tab()
    {
        static const auto key_names = []()
        {
            std::array<const char*, k_aim_keys.size()> names{};
            std::ranges::transform(k_aim_keys, names.begin(), &AimKey::name);

            return names;
        }();

        nc::BeginGroupBox("Aim-Bot");
        nc::Checkbox("Enabled", &g_aim.enabled);
        nc::Combo("Aim key:", &g_aim.key, key_names.data(), static_cast<int>(key_names.size()));
        nc::SetItemTooltip("Held to aim");
        nc::SliderFloat("Field of view:", &g_aim.cone_degrees, 1.f, 90.f, "%.0f deg");
        nc::SliderFloat("Range:", &g_aim.range, 500.f, 8000.f, "%.0f u");
        nc::SliderFloat("Smoothing:", &g_aim.smoothing, .05f, 1.f, "%.2f");
        nc::SetItemTooltip("Share of the remaining angle covered per frame, 1 snaps");
        nc::SliderFloat("Aim point:", &g_aim.head_drop, 0.f, .5f, "%.2f below top");
        nc::SetItemTooltip("For a zed whose head bone cannot be read: how far below the top of its collision box the "
                           "head is taken to be");
        nc::Checkbox("Only visible zeds", &g_aim.visible_only);

        ImGui::BeginDisabled(!g_draw_hook);
        nc::Checkbox("Silent aim", &g_aim.silent);
        ImGui::EndDisabled();
        nc::SetItemTooltip("Shots go to the head while the camera does not move. Smoothing does not apply. Hold the aim "
                           "key together with fire, or set the aim key to Left mouse");
        nc::EndGroupBox();

        nc::BeginGroupBox("Weapon");
        nc::Checkbox("No recoil", &g_aim.no_recoil);
        nc::SetItemTooltip("Firing does not kick the view up or sideways");
        nc::Checkbox("No spread", &g_aim.no_spread);
        nc::SetItemTooltip("Shots go exactly where the weapon points");
        nc::EndGroupBox();
    }

    void draw_esp_tab()
    {
        // What the colour picker edits, it works on a packed colour that has to outlive the frame
        static auto color = ImGui::ColorConvertFloat4ToU32(g_esp.color);
        static auto glow_color = ImGui::ColorConvertFloat4ToU32(g_glow.color);
        static auto ally_color = ImGui::ColorConvertFloat4ToU32(g_esp.ally_color);

        nc::BeginGroupBox("Zeds");
        nc::Checkbox("Enabled", &g_esp.zeds);
        nc::SliderFloat("Max distance:", &g_esp.max_distance, 500.f, 12000.f, "%.0f u");
        nc::SetNextItemSwatch(&color, "Box Color");
        nc::LabeledCheckbox("Box:", "Enabled", &g_esp.box);
        g_esp.color = ImGui::ColorConvertU32ToFloat4(color);
        nc::SliderFloat("Fill:", &g_esp.fill_alpha, 0.f, 1.f, "%.2f");
        nc::SetNextItemSwatch(&glow_color, "Glow Color", false);
        nc::LabeledCheckbox("Glow:", "Enabled", &g_glow.enabled);
        g_glow.color = ImGui::ColorConvertU32ToFloat4(glow_color);
        nc::SliderFloat("Glow size:", &g_glow.thickness, 1.f, 15.f, "%.1f");
        nc::Checkbox("Health bar", &g_esp.health_bar);
        nc::Checkbox("Name", &g_esp.name);
        nc::Checkbox("Health text", &g_esp.health_text);
        nc::Checkbox("Distance", &g_esp.distance);
        nc::EndGroupBox();

        nc::BeginGroupBox("Allies");
        nc::SetNextItemSwatch(&ally_color, "Ally Color", false);
        nc::LabeledCheckbox("ESP:", "Enabled", &g_esp.allies);
        nc::SetItemTooltip("Drawn with the options of the Zeds group, the name is the player's");
        g_esp.ally_color = ImGui::ColorConvertU32ToFloat4(ally_color);
        nc::Checkbox("Glow", &g_glow.allies);
        nc::Checkbox("Weapon glow", &g_glow.ally_weapons);
        nc::EndGroupBox();

        nc::BeginGroupBox("Overlay");
        nc::Checkbox("Radar", &g_esp.radar);
        nc::SliderFloat("Radar range:", &g_esp.radar_range, 500.f, 8000.f, "%.0f u");
        nc::Checkbox("World origin marker", &g_esp.origin_marker);
        nc::Checkbox("Status panel", &g_esp.status_panel);
        nc::EndGroupBox();
    }

    void draw_sensors_settings()
    {
        static constexpr std::array k_styles{"Square", "Circle"};
        static constexpr std::array k_sizes{"Small", "Medium", "Big"};

        nc::BeginGroupBox("Sensors");
        nc::Checkbox("Enabled", &g_sensors_enabled);
        nc::Combo("Style:", &g_sensors_config.Style, k_styles.data(), static_cast<int>(k_styles.size()));
        nc::Combo("Size:", &g_sensors_config.Size, k_sizes.data(), static_cast<int>(k_sizes.size()));
        nc::SliderFloat("Zoom:", &g_sensors_config.Zoom, 1.f, 30.f, "%.1f");
        nc::SetItemTooltip("World units per pixel of the radar");
        nc::Checkbox("Dynamic Scaling", &g_sensors_config.DynamicScaling);
        nc::SetItemTooltip("The radar reaches further the faster the player moves");
        nc::EndGroupBox();

        nc::BeginGroupBox("Indicators");
        nc::Checkbox("Health", &g_sensors_config.CritIndicator);
        nc::Checkbox("Features", &g_sensors_config.FeatureIndicator);
        nc::SetItemTooltip("AIM lights up while the aim key is held, KF1 has nothing behind the other rows");
        nc::Checkbox("Speed", &g_sensors_config.SpeedIndicator);
        nc::EndGroupBox();
    }

    void draw_start_menu()
    {
        if (!nc::BeginStartMenu())
            return;

        const auto origin = ImGui::GetWindowPos();
        const auto place = [origin](const float x, const float y)
        {
            ImGui::SetCursorScreenPos({origin.x + x, origin.y + y});
        };

        // The logo and the name where nullcore has the Steam avatar and the name boxes
        place(3.f, 3.f);
        nc::Image(icon_texture(g_logo), {64.f, 64.f}, {66.f, 66.f});

        nc::PushToolkitFont(2);
        place(71.f, 12.f);
        nc::Label("kf1 overlay", 0.f, 192.f);
        ImGui::PopFont();

        const auto zeds = std::format("{} zeds", g_aim_zeds.size());

        nc::PushToolkitFont(4);
        place(71.f, 36.f);
        nc::Label(zeds.c_str(), 0.f, 192.f);

        place(3.f, 71.f);

        if (nc::ImageTextButton(k_settings_window, icon_texture(g_settings_icon), {16.f, 16.f}, {160.f, 32.f}))
            nc::OpenWindow(k_settings_window);

        place(3.f, 105.f);

        if (nc::ImageTextButton(k_sensors_window, icon_texture(g_sensors_icon), {16.f, 16.f}, {160.f, 32.f}))
            nc::OpenWindow(k_sensors_window);

        ImGui::PopFont();

        nc::PushToolkitFont(6);
        place(167.f, 71.f);

        if (nc::Button("Reset Windows", {96.f, 24.f}))
            nc::ResetWindowPositions();

        place(167.f, 97.f);

        if (nc::Button("Unload", {96.f, 24.f}) && !g_unload_requested)
            request_unload();

        ImGui::PopFont();
        nc::EndStartMenu();
    }

    void draw_menu()
    {
        if (!g_desktop_ready)
        {
            g_desktop_ready = true;
            nc::OpenWindow(k_settings_window);
        }

        nc::BeginDesktop();
        draw_start_menu();
        nc::TaskBar(icon_texture(g_start_icon));

        if (nc::BeginDesktopWindow(k_settings_window, {340.f, 400.f}, 0, {20.f, 170.f}) && nc::BeginTabBar("##tabs"))
        {
            if (nc::BeginTabItem("Aim-Bot"))
            {
                draw_aim_tab();
                nc::EndTabItem();
            }

            if (nc::BeginTabItem("ESP"))
            {
                draw_esp_tab();
                nc::EndTabItem();
            }

            nc::EndTabBar();
        }

        nc::EndDesktopWindow();

        if (nc::BeginDesktopWindow(k_sensors_window, {300.f, 300.f}, 0, {380.f, 170.f}))
            draw_sensors_settings();

        nc::EndDesktopWindow();
        nc::ShowColorPickers();
        nc::EndDesktop();
    }

    // The crit gauge's states are its colours: red, orange or green by how much health is left. Crits stays -1, which
    // leaves out the readout that would call the health crits.
    void read_player_health(nc::SensorsData& data)
    {
        const auto pawn_class = kf1::read_ptr(g_player.pawn, kf1::k_object_class_offset);

        if (pawn_class != g_pawn_layout.pawn_class)
        {
            const auto offset_of = [pawn_class](const std::string_view name)
            {
                return kf1::find_property_offset(*g_target, pawn_class, name, g_classes.property);
            };

            g_pawn_layout = {pawn_class,
                             {.health = offset_of("Health"), .health_max = offset_of("HealthMax")},
                             offset_of("Velocity")};
        }

        if (!g_pawn_layout.health.health.has_value())
            return;

        const auto health = kf1::read<std::int32_t>(kf1::shift_address(g_player.pawn, *g_pawn_layout.health.health));
        const auto health_max = kf1_overlay::read_health_max(g_player.pawn, g_pawn_layout.health).value_or(100);
        const auto level = static_cast<float>(health) / static_cast<float>(health_max);

        data.Crit.State = level < .3f   ? nc::SensorsCritState::NotReady
                          : level < .6f ? nc::SensorsCritState::BanRisk
                                        : nc::SensorsCritState::Ready;
        data.Crit.Bucket = level;
        data.Crit.Cost = -1.f;
    }

    // The sensors work in Source's right handed axes and Unreal's are left handed, so the world goes in mirrored: Y and
    // the yaw change sign, which puts everything back on the side of the player it is on
    void draw_sensors(const std::vector<kf1_overlay::LiveZed>& zeds)
    {
        if (!g_sensors_enabled)
            return;

        nc::SensorsData data;
        std::vector<nc::SensorsBlip> blips;

        if (g_aim.enabled && (GetAsyncKeyState(k_aim_keys[g_aim.key].code) & 0x8000))
            data.Features = nc::SensorsFeature_AimBot;

        if (g_player.valid())
        {
            const auto state = kf1::read_camera(g_player);
            const auto add_blips = [&blips](const std::vector<kf1_overlay::LiveZed>& actors, const ImVec4& color)
            {
                const auto outline = ImGui::ColorConvertFloat4ToU32(color);

                for (const auto& actor : actors)
                {
                    nc::SensorsBlip blip;
                    blip.x = static_cast<float>(actor.position.x);
                    blip.y = static_cast<float>(-actor.position.y);
                    blip.Outline = outline;
                    blip.Fill = (outline & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 90);
                    blips.push_back(blip);
                }
            };

            data.HasLocalPlayer = true;
            data.LocalPlayerDead = !g_player.pawn;
            data.Eye = {static_cast<float>(state.position.x), static_cast<float>(-state.position.y)};
            data.Pitch = static_cast<float>(state.angles.pitch.as_degrees());
            data.Yaw = static_cast<float>(-state.angles.yaw.as_degrees());

            add_blips(zeds, g_esp.color);

            if (g_esp.allies)
                add_blips(g_allies, g_esp.ally_color);

            data.Blips = blips.data();
            data.BlipCount = static_cast<int>(blips.size());
        }

        if (g_player.pawn)
        {
            read_player_health(data);

            if (g_pawn_layout.velocity.has_value())
            {
                const auto velocity =
                        kf1::read<std::array<float, 3>>(kf1::shift_address(g_player.pawn, *g_pawn_layout.velocity));

                data.Velocity = {velocity[0], -velocity[1]};
            }
        }

        nc::Sensors(g_sensors_config, data, g_menu_open);
    }

    void draw_frame(const kf1_overlay::WindowRect& rect)
    {
        // The engine's draw normally did this already, a frame without one must not leave the camera on a head
        restore_view();
        g_aim_zeds.clear();
        g_allies.clear();
        g_ally_weapons.clear();

        set_stage("game_ready");

        if (!game_ready())
        {
            kf1_overlay::draw_status("kf1 overlay\nwaiting for the engine to register its classes");
            return;
        }

        // Cheap validation every frame, the expensive lookup only happens once the player is gone or has changed,
        // which is what a level change or a respawn looks like
        set_stage("find_local_player");
        kf1_overlay::refresh_local_player(*g_target, g_classes, g_player, g_last_player_search);

        set_stage("track_zeds");
        g_tracker.advance(*g_target, g_classes);

        static auto reused_classes = 0uz;

        if (g_tracker.reused_classes() != reused_classes)
        {
            reused_classes = g_tracker.reused_classes();
            note(g_tracker.last_reused_class());
        }

        set_stage("read_zeds");
        const auto zeds = kf1_overlay::read_living_zeds(g_tracker, g_player);
        g_allies = kf1_overlay::read_living_allies(g_tracker, g_player);
        g_ally_weapons = kf1_overlay::read_ally_weapons(g_tracker, g_allies);

        set_stage("draw_overlay");
        kf1_overlay::draw_overlay(rect, g_player, g_tracker, zeds, g_allies, g_esp);

        g_aim_zeds = zeds;

        set_stage("sensors");
        draw_sensors(zeds);

        set_stage("recoil");

        if (g_aim.no_recoil)
            remove_recoil();
        else
            restore_recoil();

        set_stage("spread");

        if (g_aim.no_spread)
            remove_spread();
        else
            restore_spread();

        static auto first_frame = true;

        if (first_frame)
        {
            first_frame = false;
            note(std::format("first overlay frame {}x{}, player valid {}", rect.width, rect.height, g_player.valid()));
        }
    }

    // ImGui's own idea of the display size is the window's client area, which in fullscreen can lag behind the mode
    // the game switched to. The back buffer is what is actually drawn into.
    [[nodiscard]]
    std::optional<ImVec2> back_buffer_size(IDirect3DDevice9* device)
    {
        IDirect3DSurface9* back_buffer{};

        if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)))
            return std::nullopt;

        D3DSURFACE_DESC description{};
        const auto described = SUCCEEDED(back_buffer->GetDesc(&description));
        back_buffer->Release();

        if (!described)
            return std::nullopt;

        return ImVec2{static_cast<float>(description.Width), static_cast<float>(description.Height)};
    }

    // At Present the game may still have some other render target bound, and in exclusive fullscreen draws made
    // outside a scene are dropped, so ImGui gets the back buffer and a scene of its own. The game's target and
    // viewport are put back afterwards.
    void render_imgui(IDirect3DDevice9* device)
    {
        IDirect3DSurface9* back_buffer{};

        if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)))
            return;

        IDirect3DSurface9* previous_target{};
        D3DVIEWPORT9 previous_viewport{};
        device->GetRenderTarget(0, &previous_target);
        device->GetViewport(&previous_viewport);
        device->SetRenderTarget(0, back_buffer);

        if (SUCCEEDED(device->BeginScene()))
        {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            device->EndScene();
        }

        if (previous_target)
        {
            device->SetRenderTarget(0, previous_target);
            previous_target->Release();
        }

        device->SetViewport(&previous_viewport);
        back_buffer->Release();
    }

    void prepare_glow(kf1_overlay::ActorGlow& glow, IDirect3DDevice9* device, const ImVec2& size)
    {
        if (glow.prepare(device, static_cast<UINT>(size.x), static_cast<UINT>(size.y)))
            return;

        static auto reported = false;

        if (!reported)
        {
            reported = true;
            note(std::format("glow off: {}", glow.error()));
        }
    }

    void present_frame(IDirect3DDevice9* device)
    {
        ++g_frame;

        static auto first_present = true;

        if (first_present)
        {
            first_present = false;
            note("first Present reached the overlay");
        }

        if (g_unload_requested)
        {
            if (g_imgui_ready)
                shutdown_imgui();

            // Also when ImGui was already down, a lost device takes it down too, the unload must not wait for it
            g_imgui_shut_down = true;

            return;
        }

        // Switching to fullscreen, and coming back to it, the game throws its device away and makes a new one. ImGui
        // holds a reference to the device it was set up on, which keeps the old one alive, and an old device that still
        // owns the screen keeps the new one lost for good: the game freezes. So ImGui lets go of the device on the
        // first sign of either, a different device or a lost one, and is set up again once the device presenting is
        // usable. Letting go also frees ImGui's D3DPOOL_DEFAULT resources, which a Reset needs gone.
        const auto cooperative = device->TestCooperativeLevel();

        if (g_imgui_ready && (device != g_imgui_device || cooperative != D3D_OK))
        {
            note(std::format("releasing ImGui: device {:X} -> {:X}, cooperative level 0x{:08X}",
                             reinterpret_cast<std::uintptr_t>(g_imgui_device), reinterpret_cast<std::uintptr_t>(device),
                             static_cast<std::uint32_t>(cooperative)));
            shutdown_imgui();
        }

        if (cooperative != D3D_OK)
            return;

        if (!g_imgui_ready)
        {
            set_stage("init_imgui");
            init_imgui(device);
            return;
        }

        static auto last_memory_note = std::chrono::steady_clock::now();

        if (const auto now = std::chrono::steady_clock::now(); now - last_memory_note >= k_memory_note_interval)
        {
            last_memory_note = now;
            note_memory();
        }

        // GetAsyncKeyState is global, so the edges are always consumed but only count while the game is in front
        const auto end_pressed = kf1_overlay::key_pressed(VK_END, g_end_was_down);
        const auto insert_pressed = kf1_overlay::key_pressed(VK_INSERT, g_insert_was_down);

        if (GetForegroundWindow() == g_game_window)
        {
            if (end_pressed)
                request_unload();

            if (insert_pressed)
                g_menu_open = !g_menu_open;
        }

        set_stage("imgui_new_frame");
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();

        if (const auto size = back_buffer_size(device))
        {
            ImGui::GetIO().DisplaySize = *size;

            if (g_glow.enabled)
                prepare_glow(g_zed_glow, device, *size);

            if (g_glow.allies)
                prepare_glow(g_ally_glow, device, *size);

            if (g_glow.ally_weapons)
                prepare_glow(g_ally_weapon_glow, device, *size);
        }

        ImGui::NewFrame();

        // The back buffer is what the game renders into, so its size is exactly the viewport the projection needs
        const auto display = ImGui::GetIO().DisplaySize;
        const kf1_overlay::WindowRect rect{0, 0, static_cast<int>(display.x), static_cast<int>(display.y)};

        // The software cursor stands in for the one the game hides while it holds the mouse
        ImGui::GetIO().MouseDrawCursor = g_menu_open;

        // A zero sized back buffer, a minimized window or a device in reset, has no viewport to project into
        if (rect.width > 0 && rect.height > 0)
            draw_frame(rect);

        if (g_menu_open)
        {
            set_stage("menu");
            draw_menu();
        }

        set_stage("imgui_render");
        ImGui::Render();
        render_imgui(device);
        set_stage("idle");
    }

    // Kept out of on_present, a __try cannot sit in a function that has objects to unwind
    void on_fault_caught()
    {
        g_faulted = true;
        note("fault caught, see kf1_overlay_fault.log, the overlay is off");
    }

    // A C++ exception leaving the hook would travel into the game, which reports every one of them as a general
    // protection fault. It stops here instead, with its message in the log.
    void present_frame_guarded(IDirect3DDevice9* device)
    {
        try
        {
            present_frame(device);
        }
        catch (const std::exception& error)
        {
            g_faulted = true;
            note(std::format("C++ exception in stage {}: {}", g_stage, error.what()));
        }
        catch (...)
        {
            g_faulted = true;
            note(std::format("unknown C++ exception in stage {}", g_stage));
        }
    }

    void aim_after_draw_guarded()
    {
        try
        {
            set_stage("aim");
            aim_at_zeds(g_aim_zeds);
            set_stage("idle");
        }
        catch (const std::exception& error)
        {
            g_faulted = true;
            note(std::format("C++ exception in stage {}: {}", g_stage, error.what()));
        }
    }

    // Runs on the way out of the engine's draw, outside the Present hook, so it brings its own guard
    void aim_after_draw()
    {
        if (g_faulted || g_unload_requested || !g_target.has_value())
            return;

        __try
        {
            aim_after_draw_guarded();
        }
        __except (fault_filter(GetExceptionInformation()))
        {
            on_fault_caught();
        }
    }

    struct IndexedDraw final
    {
        IDirect3DDevice9* device;
        D3DPRIMITIVETYPE type;
        INT base_vertex;
        UINT min_index;
        UINT vertices;
        UINT start_index;
        UINT primitives;
    };

    [[nodiscard]]
    HRESULT draw_indexed(const IndexedDraw& draw)
    {
        return g_draw_indexed_primitive_hook.stdcall<HRESULT>(draw.device, draw.type, draw.base_vertex, draw.min_index,
                                                              draw.vertices, draw.start_index, draw.primitives);
    }

    // Runs inside the game's own render, outside the Present hook, so it brings its own guard like aim_after_draw
    void draw_mesh_silhouette(const IndexedDraw& draw)
    {
        if (g_faulted || g_unload_requested)
            return;

        g_stage = "silhouette";

        __try
        {
            g_mesh_glow->draw_silhouette(draw.device,
                                         [&draw]()
                                         {
                                             std::ignore = draw_indexed(draw);
                                         });
        }
        __except (fault_filter(GetExceptionInformation()))
        {
            on_fault_caught();
        }

        g_stage = "idle";
    }

    void draw_cutouts(const IndexedDraw& draw)
    {
        const auto draw_again = [&draw]()
        {
            std::ignore = draw_indexed(draw);
        };

        g_zed_glow.draw_cutout(draw.device, draw_again);
        g_ally_glow.draw_cutout(draw.device, draw_again);
        g_ally_weapon_glow.draw_cutout(draw.device, draw_again);
    }

    void draw_mesh_cutouts(const IndexedDraw& draw)
    {
        if (g_faulted || g_unload_requested)
            return;

        g_stage = "cutout";

        __try
        {
            draw_cutouts(draw);
        }
        __except (fault_filter(GetExceptionInformation()))
        {
            on_fault_caught();
        }

        g_stage = "idle";
    }

    void blur_glows(IDirect3DDevice9* device)
    {
        g_zed_glow.blur(device, g_glow.thickness);
        g_ally_glow.blur(device, g_glow.thickness);
        g_ally_weapon_glow.blur(device, g_glow.thickness);
    }

    // The first one goes to the log, which is what tells a glow that does not show from one that is not drawn
    void lay_glows(IDirect3DDevice9* device)
    {
        static auto first = true;

        const auto lay = [device](kf1_overlay::ActorGlow& glow, const ImVec4& color)
        {
            return glow.lay(device, {color.x, color.y, color.z});
        };

        const auto zeds = lay(g_zed_glow, g_glow.color);
        const auto allies = lay(g_ally_glow, g_esp.ally_color);
        const auto weapons = lay(g_ally_weapon_glow, g_glow.ally_weapon_color);

        if ((zeds || allies || weapons) && first)
        {
            first = false;
            note("first glow drawn");
        }
    }

    // The device of the last draw of a glowing mesh, the engine's render hands out no device of its own. Null when the
    // level being drawn had none in it.
    IDirect3DDevice9* g_silhouette_device{};

    using GlowStep = void (*)(IDirect3DDevice9*);

    void run_glow_step(const GlowStep step, IDirect3DDevice9* device)
    {
        if (g_faulted || g_unload_requested)
            return;

        g_stage = "glow";

        __try
        {
            step(device);
        }
        __except (fault_filter(GetExceptionInformation()))
        {
            on_fault_caught();
        }

        g_stage = "idle";
    }

    // HUDKillingFloor::DrawModOverlay's tile, the draw that multiplies the frame: OB_Modulate is 2 * source * frame,
    // which no other pair of blend factors gives. The glows go on the frame it left behind, see kf1_actor_glow.hpp.
    void watch_for_overlay(IDirect3DDevice9* device)
    {
        DWORD blending{};
        DWORD source{};
        DWORD destination{};

        device->GetRenderState(D3DRS_ALPHABLENDENABLE, &blending);
        device->GetRenderState(D3DRS_SRCBLEND, &source);
        device->GetRenderState(D3DRS_DESTBLEND, &destination);

        if (!blending || source != D3DBLEND_DESTCOLOR || destination != D3DBLEND_SRCCOLOR)
            return;

        static auto first = true;

        if (first)
        {
            first = false;
            note("first HUD overlay seen");
        }

        g_overlay_watch = false;
        g_hud_overlay = true;

        if (g_glows_waiting)
        {
            g_glows_waiting = false;
            run_glow_step(lay_glows, device);
        }
    }

    HRESULT __stdcall on_draw_indexed_primitive(IDirect3DDevice9* device, const D3DPRIMITIVETYPE type,
                                                const INT base_vertex, const UINT min_index, const UINT vertices,
                                                const UINT start_index, const UINT primitives)
    {
        const IndexedDraw draw{device, type, base_vertex, min_index, vertices, start_index, primitives};
        const auto result = draw_indexed(draw);

        if (g_mesh_glow && SUCCEEDED(result))
        {
            g_silhouette_device = device;
            draw_mesh_silhouette(draw);
        }
        else if (g_mesh_cutout && SUCCEEDED(result))
            draw_mesh_cutouts(draw);

        if (g_overlay_watch)
            watch_for_overlay(device);

        return result;
    }

    HRESULT __stdcall on_draw_primitive(IDirect3DDevice9* device, const D3DPRIMITIVETYPE type, const UINT start_vertex,
                                        const UINT primitives)
    {
        const auto result = g_draw_primitive_hook.stdcall<HRESULT>(device, type, start_vertex, primitives);

        if (g_overlay_watch)
            watch_for_overlay(device);

        return result;
    }

    // The living zeds and allies and what the allies hold are the only actors with a glow, so a corpse goes without one
    [[nodiscard]]
    kf1_overlay::ActorGlow* glow_of(const std::uintptr_t actor)
    {
        const auto is_actor = [actor](const kf1_overlay::LiveZed& live)
        {
            return live.actor->address == actor;
        };

        if (g_glow.enabled && std::ranges::any_of(g_aim_zeds, is_actor))
            return &g_zed_glow;

        if (g_glow.allies && std::ranges::any_of(g_allies, is_actor))
            return &g_ally_glow;

        if (g_glow.ally_weapons && std::ranges::contains(g_ally_weapons, actor))
            return &g_ally_weapon_glow;

        return nullptr;
    }

    // The FDynamicActor a mesh is rendered for starts with its actor, as USkeletalMeshInstance::Render reads it.
    // __fastcall stands in for __thiscall as on_engine_draw's does.
    void __fastcall on_mesh_render(void* self, void*, void* dynamic_actor, void* scene_node, void* lights,
                                   void* projectors, void* render_interface)
    {
        g_mesh_glow = g_level_render_depth > 0 ? glow_of(*static_cast<const std::uintptr_t*>(dynamic_actor)) : nullptr;
        g_mesh_cutout = g_level_render_depth == 0 && g_glows_waiting;

        g_mesh_render_hook.thiscall<void>(self, dynamic_actor, scene_node, lights, projectors, render_interface);
        g_mesh_glow = nullptr;
        g_mesh_cutout = false;
    }

    // Whatever the last frame left undone. An overlay that did not turn up is one this level or the player's state
    // does without, the next glow then does not wait for it.
    void begin_level_render()
    {
        if (g_overlay_watch)
        {
            g_overlay_watch = false;
            g_hud_overlay = false;
        }

        if (!g_glows_waiting)
            return;

        g_glows_waiting = false;
        g_zed_glow.discard();
        g_ally_glow.discard();
        g_ally_weapon_glow.discard();
    }

    // Without the HUD's overlay nothing multiplies the frame, so the glows can go on right here
    void end_level_render()
    {
        g_overlay_watch = true;

        if (!g_silhouette_device)
            return;

        run_glow_step(blur_glows, g_silhouette_device);

        if (g_hud_overlay)
            g_glows_waiting = true;
        else
            run_glow_step(lay_glows, g_silhouette_device);

        g_silhouette_device = nullptr;
    }

    // The level is done here and the first person weapon and the HUD are still to come, FPlayerSceneNode::Render calls
    // this right before them
    void __fastcall on_level_render(void* self, void*, void* render_interface)
    {
        if (g_level_render_depth == 0)
            begin_level_render();

        ++g_level_render_depth;
        g_level_render_hook.thiscall<void>(self, render_interface);

        if (--g_level_render_depth == 0)
            end_level_render();
    }

    void hook_engine_export(safetyhook::InlineHook& hook, const char* name, void* detour)
    {
        const auto engine = GetModuleHandleW(L"Engine.dll");
        const auto function = engine ? GetProcAddress(engine, name) : nullptr;

        if (!function)
        {
            note(std::format("{} not found, no zed glow", name));
            return;
        }

        auto created = safetyhook::InlineHook::create(function, detour);

        if (created)
            hook = std::move(*created);

        note(std::format("hook {} {}", name, created ? "ok" : "FAILED"));
    }

    // The game's own device gives the address: d3d9.dll has a function per kind of device
    void hook_draw_indexed_primitive(IDirect3DDevice9* device)
    {
        constexpr std::size_t k_draw_primitive_slot = 81;
        constexpr std::size_t k_draw_indexed_primitive_slot = 82;

        if (g_draw_indexed_primitive_hook)
            return;

        const auto hook = [device](safetyhook::InlineHook& target, const std::size_t slot, void* detour, const char* name)
        {
            auto created = safetyhook::InlineHook::create((*reinterpret_cast<void***>(device))[slot], detour);

            if (created)
                target = std::move(*created);

            note(std::format("hook {} {}", name, created ? "ok" : "FAILED"));
        };

        hook(g_draw_primitive_hook, k_draw_primitive_slot, reinterpret_cast<void*>(on_draw_primitive), "DrawPrimitive");
        hook(g_draw_indexed_primitive_hook, k_draw_indexed_primitive_slot,
             reinterpret_cast<void*>(on_draw_indexed_primitive), "DrawIndexedPrimitive");
    }

    // D3D9Drv's post effects, TripPostFX. A display change makes UD3D9RenderDevice::SetRes try a Reset, which their
    // render targets make fail, so it drops the device and makes a new one, with new post effects written over the
    // pointers to the old ones. Nothing releases those, and they keep their part of the old device: some 140 MB of
    // address space per display change, until the game runs out of it. So the release SetRes leaves out happens where
    // it is about to make the new device, the same CleanUp the game runs before it reloads the post effects itself.
    constexpr std::uintptr_t k_post_effects_clean_up_rva = 0x2120;
    constexpr std::array<std::uint8_t, 10> k_post_effects_clean_up_code{0x51, 0x53, 0x55, 0x56, 0x57,
                                                                        0x83, 0xEC, 0x1C, 0x8B, 0xF1};
    constexpr std::uintptr_t k_set_res_create_device_rva = 0x15EEE;
    constexpr std::array<std::uint8_t, 2> k_set_res_create_device_code{0x8B, 0x0D};
    constexpr std::ptrdiff_t k_render_device_post_effects_offset = 0xC0;

    using PostEffectsCleanUp = void(__thiscall*)(std::uintptr_t);

    PostEffectsCleanUp g_post_effects_clean_up{};

    // SetRes keeps the render device in esi
    void on_create_device(safetyhook::Context& context)
    {
        g_post_effects_clean_up(context.esi + k_render_device_post_effects_offset);
    }

    void hook_create_device()
    {
        auto* driver = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(L"D3D9Drv.dll"));

        if (!driver
            || std::memcmp(driver + k_post_effects_clean_up_rva, k_post_effects_clean_up_code.data(),
                           k_post_effects_clean_up_code.size())
            || std::memcmp(driver + k_set_res_create_device_rva, k_set_res_create_device_code.data(),
                           k_set_res_create_device_code.size()))
        {
            note("D3D9Drv.dll is not the known build, the post effects keep leaking");
            return;
        }

        g_post_effects_clean_up = reinterpret_cast<PostEffectsCleanUp>(driver + k_post_effects_clean_up_rva);

        auto hook = safetyhook::MidHook::create(driver + k_set_res_create_device_rva, on_create_device);

        if (hook)
            g_create_device_hook = std::move(*hook);

        note(std::format("hook SetRes device creation {}", hook ? "ok" : "FAILED"));
    }

    // The hook callback. No object with a destructor lives in this frame, which is what lets it hold the __try.
    void on_present(IDirect3DDevice9* device, const RECT*, const RECT*, const HWND, const RGNDATA*)
    {
        if (g_faulted)
            return;

        __try
        {
            present_frame_guarded(device);
        }
        __except (fault_filter(GetExceptionInformation()))
        {
            on_fault_caught();
        }
    }

    // A Reset needs every D3DPOOL_DEFAULT resource gone, and ImGui does not bring a user texture back on its own, the
    // menu's icons among them. So ImGui starts over, the next Present sets it up again.
    void on_reset(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)
    {
        if (g_imgui_ready)
            shutdown_imgui();
    }
} // namespace

BOOL WINAPI DllMain(const HINSTANCE h_instance, const DWORD reason, const LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = h_instance;
        DisableThreadLibraryCalls(h_instance);

        // Hooking waits for the game to have loaded its renderer, d3d9.dll is not there yet when the DLL is injected
        // early
        std::thread(
                []()
                {
                    note("DLL loaded, waiting for d3d9.dll");

                    while (!GetModuleHandleW(L"d3d9.dll"))
                        Sleep(100);

                    auto& manager = omath::hooks::HooksManager::get();
                    manager.set_on_dx9_present(on_present);
                    manager.set_on_dx9_reset(on_reset);
                    // A device that cannot be made right now, the adapter is busy with a mode change, may be made a
                    // moment later
                    while (!manager.hook_dx9())
                    {
                        note("hook_dx9 failed, trying again");
                        Sleep(1000);
                    }

                    note("hook_dx9 ok");
                    hook_engine_draw();
                    hook_engine_export(g_level_render_hook, "?Render@FLevelSceneNode@@UAEXPAVFRenderInterface@@@Z",
                                       reinterpret_cast<void*>(on_level_render));
                    hook_engine_export(g_mesh_render_hook,
                                       "?Render@USkeletalMeshInstance@@UAEXPAVFDynamicActor@@PAVFLevelSceneNode@@"
                                       "PAV?$TList@PAVFDynamicLight@@@@PAV?$TList@PAUFProjectorRenderInfo@@@@"
                                       "PAVFRenderInterface@@@Z",
                                       reinterpret_cast<void*>(on_mesh_render));
                    hook_create_device();
                })
                .detach();
    }
    // A non null reserved means the whole process is going down, the OS reclaims everything and unhooking under the
    // loader lock at that point is only a way to hang the exit
    else if (reason == DLL_PROCESS_DETACH && !reserved)
    {
        // END already tore everything down in order, see unload_thread. What is left is someone else unloading the DLL,
        // which cannot be made safe against a Present running at the same time but beats leaving hooks behind.
        if (!g_unload_requested)
        {
            omath::hooks::HooksManager::get().unhook_wnd_proc();
            omath::hooks::HooksManager::get().unhook_dx9();
            g_draw_hook = {};
            g_level_render_hook = {};
            g_mesh_render_hook = {};
            g_draw_indexed_primitive_hook = {};
            g_draw_primitive_hook = {};
            g_create_device_hook = {};

            if (g_imgui_ready)
            {
                g_zed_glow.release();
                g_ally_glow.release();
                g_ally_weapon_glow.release();
                ImGui_ImplDX9_Shutdown();
                ImGui_ImplWin32_Shutdown();
                free_icons();
                ImGui::DestroyContext();
            }
        }
    }

    return TRUE;
}
