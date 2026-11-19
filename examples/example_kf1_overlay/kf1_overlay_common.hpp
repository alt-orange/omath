//
// Created by Vlad on 8/14/2026.
//
// Pieces of the KF1 overlay that do not care how ImGui gets onto the screen, shared by the standalone GLFW window
// (example_kf1_overlay) and the DirectX 9 hook (example_kf1_overlay_dll).

#pragma once
#include "kf1_common/kf1_target.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <format>
#include <imgui.h>
#include <memory>
#include <omath/3d_primitives/aabb.hpp>
#include <omath/algorithm/radar.hpp>
#include <omath/engines/unreal_engine/camera.hpp>
#include <omath/hud/entity_overlay.hpp>
#include <omath/hud/renderer_realizations/imgui_renderer.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kf1_overlay
{
    using omath::projection::FieldOfView;
    using omath::projection::ViewPort;
    using omath::unreal_engine::Camera;

    constexpr auto k_near_plane = 1.;
    constexpr auto k_far_plane = 32768.;

    // A full sweep of the object table costs about 40 ms, so it is spread over frames instead of stalling one. This
    // budget keeps a sweep at roughly 4 ms per frame and refreshes the actor list about five times a second.
    constexpr std::size_t k_scan_budget = 8192;
    constexpr std::size_t k_max_tracked = 512;

    constexpr auto k_radar_radius = 110.f;
    constexpr auto k_radar_margin = 24.f;

    // The local player is looked up again at most this often, a lookup can fall back to a sweep over every object
    constexpr auto k_player_search_interval = std::chrono::seconds(1);

    // No zed has more health than this, which is what tells a HealthMax read as the right type from one read as the wrong
    // one
    constexpr int k_max_plausible_health = 100000;

    // Every zed in the game derives from this class, the base class of the whole KFChar package
    constexpr std::string_view k_zed_base_class = "KFMonster";

    // Every player and bot is one of these, except for the hostile humans some story maps have
    constexpr std::string_view k_ally_base_class = "KFHumanPawn";
    constexpr std::string_view k_hostile_human_class = "KFHumanPawnEnemy";

    // What everyone else sees of a player's weapon is an actor of its own, an instance of this
    constexpr std::string_view k_weapon_base_class = "WeaponAttachment";

    // What an ally goes by until the server has told its name
    constexpr std::string_view k_unnamed_ally = "Ally";

    // What the overlay draws. The DLL's menu changes these while the game runs, the standalone overlay keeps the
    // defaults.
    struct EspSettings final
    {
        bool zeds = true;
        // The other players, drawn with the same options as the zeds in a colour of their own
        bool allies = true;
        ImVec4 ally_color{.6f, .8f, 1.f, 1.f};
        float max_distance = 8000.f;
        bool box = true;
        ImVec4 color{.31f, .86f, .47f, 1.f};
        float fill_alpha = .12f;
        bool health_bar = true;
        bool name = true;
        bool health_text = true;
        bool distance = true;
        bool radar = true;
        float radar_range = 3000.f;
        bool origin_marker = true;
        bool status_panel = true;
        // What the labels are written in, the current ImGui font when null. It belongs to the ImGui context. A font set
        // here is nullcore's, whose glyphs bring their own black outline.
        ImFont* font = nullptr;
        // The line height font is drawn at: the 14 px cell of nullcore's ESP font (UIFONT_SMALL1) and 1 px of outline on
        // either side
        float font_size = 16.f;
    };

    struct WindowRect final
    {
        int x{};
        int y{};
        int width{};
        int height{};

        [[nodiscard]]
        bool operator==(const WindowRect&) const = default;
    };

    // Unreal is Z up
    using ZedBounds = omath::primitives::Aabb<double, omath::primitives::UpAxis::Z>;

    // Where the fields the overlay reads sit inside a zed. They are resolved by name from the first zed or ally class
    // that turns up, all of them are declared on Actor and Pawn so either kind of class gives the same answer.
    struct ZedLayout final
    {
        std::optional<std::int32_t> health;
        std::optional<std::int32_t> collision_radius;
        std::optional<std::int32_t> collision_height;
        // The health a zed spawned with, what the health bar is measured against
        std::optional<std::int32_t> health_max;
        // The actor this one is attached to
        std::optional<std::int32_t> base;
    };

    // A zed's name as the players know it. The classes are Zombie<Name>_<Event>, ZombieClot_XMas for one, and an FName
    // keeps the case it was first seen in, so the same zed can be ZombieFleshPound or ZombieFleshpound. A zed a mod adds
    // keeps what is left of its class name.
    [[nodiscard]]
    inline std::string zed_display_name(std::string_view class_name)
    {
        constexpr std::string_view k_class_prefix = "Zombie";
        constexpr std::array<std::string_view, 9> k_zed_names{"Bloat", "Clot",  "Crawler", "Fleshpound", "Gorefast",
                                                              "Husk",  "Scrake", "Siren",  "Stalker"};

        if (class_name.starts_with(k_class_prefix))
            class_name.remove_prefix(k_class_prefix.size());

        class_name = class_name.substr(0, class_name.find('_'));

        const auto is_named = [class_name](const std::string_view name)
        {
            return std::ranges::equal(class_name, name, {}, ::tolower, ::tolower);
        };

        if (is_named("Boss"))
            return "Patriarch";

        const auto known = std::ranges::find_if(k_zed_names, is_named);

        return std::string{known != k_zed_names.end() ? *known : class_name};
    }

    struct ClassInfo final
    {
        std::string name;
        bool is_zed{};
        bool is_ally{};
        bool is_weapon{};
        // UObject::Name of the class this was made for, and the last sweep that found it still there
        std::int32_t name_index{};
        std::size_t checked_sweep{};
    };

    struct TrackedActor final
    {
        std::uintptr_t address{};
        std::string name;
        // What the players call it: see zed_display_name, or an ally's player name
        std::string display_name;
        // What the actor's class pointer read when it was found, a different value later means the slot was reused
        std::uintptr_t object_class{};
    };

    // Walks the object table a slice at a time and collects the zeds: every instance of a class that has KFMonster
    // somewhere up its chain. There is no list of zed classes to keep up to date, a zed a mod adds is picked up as is.
    // The players are collected the same way, in a list of their own: nothing that goes through the zeds may reach one.
    class ActorTracker final
    {
    public:
        void advance(const kf1::Target& target, const kf1::ReflectionClasses& classes)
        {
            if (m_cursor >= m_table.size())
                start_sweep(target);

            const auto end = std::min(m_cursor + k_scan_budget, m_table.size());

            for (; m_cursor < end; ++m_cursor)
            {
                const std::uintptr_t object = m_table[m_cursor];

                if (!object)
                    continue;

                const auto object_class = kf1::read_ptr(object, kf1::k_object_class_offset);

                if (!object_class)
                    continue;

                const auto& info = class_info(target, classes, object_class);

                if (info.is_ally)
                {
                    m_allies_in_progress.push_back({object, kf1::object_name(target, object),
                                                    ally_name(target, classes, object), object_class});
                    continue;
                }

                if (info.is_weapon)
                {
                    m_weapons_in_progress.push_back({object, {}, {}, object_class});
                    continue;
                }

                if (!info.is_zed)
                    continue;

                if (m_in_progress.size() >= k_max_tracked)
                {
                    m_capped_in_progress = true;
                    continue;
                }

                // The object name is read once per sweep here, reading it per frame would cost far more
                m_in_progress.push_back(
                        {object, kf1::object_name(target, object), zed_display_name(info.name), object_class});
            }
        }

        [[nodiscard]]
        const std::vector<TrackedActor>& tracked() const
        {
            return m_tracked;
        }

        // Every player, the local one among them
        [[nodiscard]]
        const std::vector<TrackedActor>& tracked_allies() const
        {
            return m_tracked_allies;
        }

        // Every weapon in someone's hands, see read_ally_weapons. Nothing but the address and the class is filled in.
        [[nodiscard]]
        const std::vector<TrackedActor>& tracked_weapons() const
        {
            return m_tracked_weapons;
        }

        // True when there are more zeds than k_max_tracked and the rest are left out
        [[nodiscard]]
        bool capped() const
        {
            return m_capped;
        }

        // How many zed or ally classes turned up where another class used to be, and the last of them
        [[nodiscard]]
        std::size_t reused_classes() const
        {
            return m_reused_classes;
        }

        [[nodiscard]]
        const std::string& last_reused_class() const
        {
            return m_last_reused_class;
        }

        // Known once the first zed class has been met. Health is what tells a zed from the corpse it leaves behind, the
        // collision cylinder is what its box is made of.
        [[nodiscard]]
        const ZedLayout& zed_layout() const
        {
            return m_zed_layout;
        }

    private:
        void start_sweep(const kf1::Target& target)
        {
            m_tracked = std::move(m_in_progress);
            m_in_progress.clear();
            m_tracked_allies = std::move(m_allies_in_progress);
            m_allies_in_progress.clear();
            m_tracked_weapons = std::move(m_weapons_in_progress);
            m_weapons_in_progress.clear();
            m_capped = m_capped_in_progress;
            m_capped_in_progress = false;
            m_table = kf1::read_object_table(target);
            m_cursor = 0;
            ++m_sweep;
        }

        // Naming and classifying a class is the expensive part, so it is done once and then only looked up. A class
        // does not stay put for good though: loading a level frees every script class, and the next level's land on
        // whatever addresses are free, which can be where a class of the menus used to be. So each sweep checks the
        // name of what it looks up once, an entry for the wrong class would hide every zed of the right one.
        [[nodiscard]]
        const ClassInfo& class_info(const kf1::Target& target, const kf1::ReflectionClasses& classes,
                                    const std::uintptr_t object_class)
        {
            const auto name_address = kf1::shift_address(object_class, target.names.object_name_offset);
            const auto known = m_classes.find(object_class);
            std::string replaced;

            if (known != m_classes.end())
            {
                if (known->second.checked_sweep == m_sweep)
                    return known->second;

                if (kf1::read<std::int32_t>(name_address) == known->second.name_index)
                {
                    known->second.checked_sweep = m_sweep;
                    return known->second;
                }

                replaced = std::move(known->second.name);
                m_classes.erase(known);
            }

            const auto zed_base = find_base_class(target, object_class, k_zed_base_class);
            const auto ally_base = zed_base || find_base_class(target, object_class, k_hostile_human_class)
                                           ? 0
                                           : find_base_class(target, object_class, k_ally_base_class);
            const auto layout_class = zed_base ? zed_base : ally_base;

            if (layout_class && !m_zed_layout.health.has_value())
                m_zed_layout = {kf1::find_property_offset(target, layout_class, "Health", classes.property),
                                kf1::find_property_offset(target, layout_class, "CollisionRadius", classes.property),
                                kf1::find_property_offset(target, layout_class, "CollisionHeight", classes.property),
                                kf1::find_property_offset(target, layout_class, "HealthMax", classes.property),
                                kf1::find_property_offset(target, layout_class, "Base", classes.property)};

            const auto is_weapon = find_base_class(target, object_class, k_weapon_base_class) != 0;
            const auto& info = m_classes
                                       .emplace(object_class,
                                                ClassInfo{kf1::object_name(target, object_class), zed_base != 0,
                                                          ally_base != 0, is_weapon,
                                                          kf1::read<std::int32_t>(name_address), m_sweep})
                                       .first->second;

            if (!replaced.empty() && (info.is_zed || info.is_ally))
            {
                m_last_reused_class = std::format("{} took over the address of {}", info.name, replaced);
                ++m_reused_classes;
            }

            return info;
        }

        // The named class object when it is the class itself or one of its parents, 0 otherwise. The hop guard keeps
        // corrupt data from spinning forever.
        [[nodiscard]]
        static std::uintptr_t find_base_class(const kf1::Target& target, std::uintptr_t object_class,
                                              const std::string_view name)
        {
            for (auto hops = 0; object_class && hops < 64; ++hops)
            {
                if (kf1::object_name(target, object_class) == name)
                    return object_class;

                object_class = kf1::read_ptr(object_class, kf1::k_struct_super_offset);
            }

            return 0;
        }

        // Pawn::PlayerReplicationInfo::PlayerName. The replication info reaches a client some time after the pawn, the
        // next sweep then finds it.
        [[nodiscard]]
        std::string ally_name(const kf1::Target& target, const kf1::ReflectionClasses& classes,
                              const std::uintptr_t pawn)
        {
            if (!m_replication_info_offset.has_value())
                m_replication_info_offset =
                        kf1::find_property_offset(target, kf1::read_ptr(pawn, kf1::k_object_class_offset),
                                                  "PlayerReplicationInfo", classes.property);

            const auto info = m_replication_info_offset.has_value() ? kf1::read_ptr(pawn, *m_replication_info_offset) : 0;

            if (!info)
                return std::string{k_unnamed_ally};

            if (!m_player_name_offset.has_value())
                m_player_name_offset = kf1::find_property_offset(
                        target, kf1::read_ptr(info, kf1::k_object_class_offset), "PlayerName", classes.property);

            auto name = m_player_name_offset.has_value() ? kf1::read_string(info + *m_player_name_offset) : std::string{};

            return name.empty() ? std::string{k_unnamed_ally} : name;
        }

        std::vector<std::uint32_t> m_table;
        std::size_t m_cursor{};
        std::size_t m_sweep{};
        std::size_t m_reused_classes{};
        std::string m_last_reused_class;
        std::unordered_map<std::uintptr_t, ClassInfo> m_classes;
        ZedLayout m_zed_layout;
        std::vector<TrackedActor> m_in_progress;
        std::vector<TrackedActor> m_tracked;
        std::vector<TrackedActor> m_allies_in_progress;
        std::vector<TrackedActor> m_tracked_allies;
        std::vector<TrackedActor> m_weapons_in_progress;
        std::vector<TrackedActor> m_tracked_weapons;
        std::optional<std::int32_t> m_replication_info_offset;
        std::optional<std::int32_t> m_player_name_offset;
        bool m_capped_in_progress{};
        bool m_capped{};
    };

    // Keeps the local player current. A controller class check alone is not enough: the controller behind the main menu
    // backdrop passes it long after the level changed under it, and the camera then reads a dead object and sits at the
    // world origin. The pawn the engine listens from is what changes with the level, so a different one that a player
    // controls means the player has to be found again, and the same goes for a respawn. The lookup itself is rate
    // limited, it can sweep every object.
    //
    // Dead or spectating there is no pawn of the player's own, and the engine listens from whatever is being watched.
    // Neither is a reason to search: no search finds anything better then, and one that fails sweeps every object, which
    // once a second is a stutter.
    inline void refresh_local_player(const kf1::Target& target, const kf1::ReflectionClasses& classes,
                                     kf1::LocalPlayer& player, std::chrono::steady_clock::time_point& last_search)
    {
        // A pawn the controller has left, the one it died in, is simply let go of
        if (player.pawn && kf1::read_ptr(player.pawn, kf1::k_actor_controller_offset) != player.controller)
            player.pawn = 0;

        const auto listener = kf1::audio_listener(target);
        const auto listener_controller = listener ? kf1::read_ptr(listener, kf1::k_actor_controller_offset) : 0;
        const auto new_player_pawn =
                listener != player.pawn && listener_controller
                && kf1::is_kind_of(kf1::read_ptr(listener_controller, kf1::k_object_class_offset),
                                   classes.player_controller)
                && kf1::is_local_controller(target, listener_controller);

        const auto stale = !kf1::still_valid(classes, player) || new_player_pawn;
        const auto now = std::chrono::steady_clock::now();

        if (!stale || now - last_search < k_player_search_interval)
            return;

        last_search = now;
        player = kf1::find_local_player(target, classes);
    }

    [[nodiscard]]
    inline bool key_pressed(const int key, bool& was_down)
    {
        const auto down = (GetAsyncKeyState(key) & 0x8000) != 0;
        const auto pressed = down && !was_down;
        was_down = down;

        return pressed;
    }

    inline void draw_origin_marker(const ImVec2& position, const double distance)
    {
        auto* canvas = ImGui::GetBackgroundDrawList();

        canvas->AddCircleFilled(position, 4.f, IM_COL32(255, 170, 0, 255));
        canvas->AddCircle(position, 14.f, IM_COL32(255, 170, 0, 200), 0, 2.f);
        canvas->AddLine({position.x - 24.f, position.y}, {position.x - 16.f, position.y}, IM_COL32(255, 170, 0, 200),
                        2.f);
        canvas->AddLine({position.x + 16.f, position.y}, {position.x + 24.f, position.y}, IM_COL32(255, 170, 0, 200),
                        2.f);
        canvas->AddText({position.x + 20.f, position.y - 8.f}, IM_COL32(255, 255, 255, 255),
                        std::format("world origin {{0, 0, 0}}\n{:.0f} units away", distance).c_str());
    }

    // A zed's collision cylinder is its box: Location is the middle of it, CollisionRadius reaches out sideways and
    // CollisionHeight is half of the height.
    [[nodiscard]]
    inline std::optional<ZedBounds> read_zed_bounds(const std::uintptr_t actor, const ZedLayout& layout)
    {
        if (!layout.collision_radius.has_value() || !layout.collision_height.has_value())
            return std::nullopt;

        const auto radius = static_cast<double>(kf1::read<float>(kf1::shift_address(actor, *layout.collision_radius)));
        const auto half_height =
                static_cast<double>(kf1::read<float>(kf1::shift_address(actor, *layout.collision_height)));

        // Garbage from a freed actor must not turn into a screen sized box
        if (!(radius > 0. && radius < 1000. && half_height > 0. && half_height < 1000.))
            return std::nullopt;

        const auto center = kf1::read_location(actor);

        return ZedBounds{{center.x - radius, center.y - radius, center.z - half_height},
                         {center.x + radius, center.y + radius, center.z + half_height}};
    }

    // KFMonster::HealthMax. Whether the game declares it as an int or a float is not something to rely on, and the two
    // readings cannot be mistaken for each other: an int 130 is a denormal float, a float 130.0 is over a billion as
    // an int. Whichever one lands in the plausible range is the real one.
    [[nodiscard]]
    inline std::optional<int> read_health_max(const std::uintptr_t actor, const ZedLayout& layout)
    {
        if (!layout.health_max.has_value())
            return std::nullopt;

        const auto address = kf1::shift_address(actor, *layout.health_max);
        const auto as_int = kf1::read<std::int32_t>(address);

        if (as_int >= 1 && as_int <= k_max_plausible_health)
            return as_int;

        const auto as_float = kf1::read<float>(address);

        if (as_float >= 1.f && as_float <= static_cast<float>(k_max_plausible_health))
            return static_cast<int>(as_float);

        return std::nullopt;
    }

    // A zed or an ally that is alive right now, with everything that is read about it once per frame
    struct LiveZed final
    {
        const TrackedActor* actor{};
        omath::Vector3<double> position;
        std::optional<ZedBounds> bounds;
        std::optional<int> health;
        std::optional<int> health_max;
    };

    // The tracked actors that are still there and still alive. A tracked actor may have been destroyed since the sweep
    // found it, its memory then belongs to someone else, and a killed zed stays around as a corpse for a while.
    //
    // The object table also holds more than the world does. A destroyed actor stays in it, frozen with its last position
    // and health, until the next garbage collection: a player the server stops sending is destroyed here and comes back
    // as a new actor, which makes two of them for a while. The actors of the previous level can outlive it the same way.
    [[nodiscard]]
    inline std::vector<LiveZed> read_living(const std::vector<TrackedActor>& tracked, const ZedLayout& layout,
                                            const std::uintptr_t level)
    {
        std::vector<LiveZed> zeds;

        for (const auto& actor : tracked)
        {
            if (kf1::read_ptr(actor.address, kf1::k_object_class_offset) != actor.object_class)
                continue;

            const auto flags = kf1::read<std::uint32_t>(kf1::shift_address(actor.address, kf1::k_actor_flags_offset));

            if (flags & kf1::k_actor_deleted_flag || kf1::read_ptr(actor.address, kf1::k_actor_level_offset) != level)
                continue;

            std::optional<int> health;

            if (layout.health.has_value())
                health = kf1::read<std::int32_t>(kf1::shift_address(actor.address, *layout.health));

            if (health.has_value() && *health <= 0)
                continue;

            zeds.push_back({&actor, kf1::read_location(actor.address), read_zed_bounds(actor.address, layout), health,
                            read_health_max(actor.address, layout)});
        }

        return zeds;
    }

    [[nodiscard]]
    inline std::vector<LiveZed> read_living_zeds(const ActorTracker& tracker, const kf1::LocalPlayer& player)
    {
        return read_living(tracker.tracked(), tracker.zed_layout(),
                           kf1::read_ptr(player.controller, kf1::k_actor_level_offset));
    }

    // The player's own pawn is left out, it is what the camera sits in
    [[nodiscard]]
    inline std::vector<LiveZed> read_living_allies(const ActorTracker& tracker, const kf1::LocalPlayer& player)
    {
        auto allies = read_living(tracker.tracked_allies(), tracker.zed_layout(),
                                  kf1::read_ptr(player.controller, kf1::k_actor_level_offset));

        std::erase_if(allies,
                      [&player](const LiveZed& ally)
                      {
                          return ally.actor->address == player.pawn;
                      });

        return allies;
    }

    // The weapons these allies hold. AActor::AttachToBone makes the pawn the Base of what it attaches, and dual pistols
    // are two attachments.
    [[nodiscard]]
    inline std::vector<std::uintptr_t> read_ally_weapons(const ActorTracker& tracker, const std::vector<LiveZed>& allies)
    {
        std::vector<std::uintptr_t> weapons;
        const auto& base = tracker.zed_layout().base;

        if (!base.has_value())
            return weapons;

        for (const auto& weapon : tracker.tracked_weapons())
        {
            if (kf1::read_ptr(weapon.address, kf1::k_object_class_offset) != weapon.object_class)
                continue;

            const auto holder = kf1::read_ptr(weapon.address, *base);
            const auto held_by_ally = std::ranges::any_of(allies,
                                                          [holder](const LiveZed& ally)
                                                          {
                                                              return ally.actor->address == holder;
                                                          });

            if (held_by_ally)
                weapons.push_back(weapon.address);
        }

        return weapons;
    }

    // The ESP style box around one zed or ally, built by omath from the projected corners of its AABB. False when a
    // corner cannot be projected, an actor half behind the camera has no meaningful rectangle.
    inline bool draw_actor_box(const Camera& camera, const LiveZed& live, const ZedBounds& bounds, const double distance,
                               const ImVec4& color, const EspSettings& settings)
    {
        const auto& actor = *live.actor;
        const auto& health = live.health;
        const auto& health_max = live.health_max;

        using namespace omath::hud::widget;
        using omath::Color;

        static const std::shared_ptr<omath::hud::HudRendererInterface> renderer =
                std::make_shared<omath::hud::ImguiHudRenderer>();

        auto overlay = omath::hud::EntityOverlay::from_aabb(camera, bounds, 4.f, renderer);

        if (!overlay.has_value())
            return false;

        const Color main{color.x, color.y, color.z, color.w};
        const Color fill{color.x, color.y, color.z, settings.fill_alpha};
        const Color red{.9f, .2f, .2f, 1.f};
        const Color white{1.f, 1.f, 1.f, 1.f};
        const Color black{0.f, 0.f, 0.f, 1.f};
        const Color bar_background{.1f, .1f, .1f, .8f};
        const auto outlined = settings.font ? Outlined::Off : Outlined::On;

        // Label texts are not owned by the widgets, they have to outlive the contents() call below
        const auto health_text = health.has_value() ? std::format("{} HP", *health) : std::string{};
        const auto distance_text = std::format("{:.0f}u", distance);
        const auto health_ratio = health.has_value() && health_max.has_value()
                                          ? std::clamp(static_cast<float>(*health) / static_cast<float>(*health_max),
                                                       0.f, 1.f)
                                          : 1.f;

        overlay->contents(
                omath::hud::when(settings.box, CorneredBox{main, fill, black}),
                omath::hud::when(settings.health_bar && health_max.has_value(),
                                 LeftSide{Bar{BarPaint{BarGradient{red, main}}, black, bar_background, 4.f,
                                              health_ratio}}),
                omath::hud::when(settings.name,
                                 TopSide{Centered{Label{Paint{white}, 2.f, outlined, actor.display_name}}}),
                omath::hud::when(settings.health_text && health.has_value(),
                                 RightSide{Label{Paint{main}, 2.f, outlined, health_text}}),
                omath::hud::when(settings.distance,
                                 BottomSide{Centered{Label{Paint{white}, 2.f, outlined, distance_text}}}));

        return true;
    }

    // The radar sits in the top right corner of the game's client area, north on it is wherever the player looks
    [[nodiscard]]
    inline ImVec2 radar_center(const WindowRect& rect)
    {
        return {static_cast<float>(rect.width) - k_radar_margin - k_radar_radius, k_radar_margin + k_radar_radius};
    }

    inline void draw_radar_background(const WindowRect& rect, const float range)
    {
        auto* canvas = ImGui::GetBackgroundDrawList();
        const auto center = radar_center(rect);

        canvas->AddCircleFilled(center, k_radar_radius, IM_COL32(0, 0, 0, 120));
        canvas->AddCircle(center, k_radar_radius, IM_COL32(210, 210, 210, 90), 0, 1.5f);
        canvas->AddCircle(center, k_radar_radius * .5f, IM_COL32(210, 210, 210, 40), 0, 1.f);
        canvas->AddLine({center.x - k_radar_radius, center.y}, {center.x + k_radar_radius, center.y},
                        IM_COL32(210, 210, 210, 40));
        canvas->AddLine({center.x, center.y - k_radar_radius}, {center.x, center.y + k_radar_radius},
                        IM_COL32(210, 210, 210, 40));

        // The player always sits in the middle looking up the screen
        canvas->AddTriangleFilled({center.x, center.y - 7.f}, {center.x - 5.f, center.y + 5.f},
                                  {center.x + 5.f, center.y + 5.f}, IM_COL32(255, 255, 255, 220));
        canvas->AddText({center.x - k_radar_radius, center.y + k_radar_radius + 4.f}, IM_COL32(210, 210, 210, 140),
                        std::format("radar {:.0f}u", range).c_str());
    }

    // omath turns a world position into an offset from the radar centre: forward is -y, right is +x, which is already
    // what ImGui wants. Anything past the radar range is pinned to the rim so its direction still shows.
    inline void draw_radar_blip(const WindowRect& rect, const Camera& camera, const omath::Vector3<double>& position,
                         const float range, const ImU32 color)
    {
        auto offset = omath::algorithm::world_to_radar(camera, position, static_cast<double>(k_radar_radius / range));
        const auto length = offset.length();
        const auto pinned = length > k_radar_radius;

        if (pinned && length > 0.f)
            offset *= k_radar_radius / length;

        const auto center = radar_center(rect);

        ImGui::GetBackgroundDrawList()->AddCircleFilled({center.x + offset.x, center.y + offset.y}, pinned ? 2.f : 3.f,
                                                        color);
    }

    inline void draw_status(const std::string& text)
    {
        auto* canvas = ImGui::GetBackgroundDrawList();

        canvas->AddRectFilled({12.f, 12.f}, {392.f, 152.f}, IM_COL32(0, 0, 0, 140), 6.f);
        canvas->AddText({24.f, 24.f}, IM_COL32(220, 220, 220, 255), text.c_str());
    }

    // One frame of the overlay: camera readout, world origin marker, radar, every living zed and every ally
    inline void draw_overlay(const WindowRect& rect, const kf1::LocalPlayer& player, const ActorTracker& tracker,
                             const std::vector<LiveZed>& zeds, const std::vector<LiveZed>& allies,
                             const EspSettings& settings)
    {
        if (player.valid())
        {
            const auto state = kf1::read_camera(player);
            const auto view_port = ViewPort{static_cast<float>(rect.width), static_cast<float>(rect.height)};

            // Everything the projection needs comes straight out of the game: origin, angles, and the horizontal FOV
            // that UE keeps in PlayerController::FovAngle
            const Camera camera{state.position, state.angles,
                                view_port,      FieldOfView::from_degrees(static_cast<float>(state.fov)),
                                k_near_plane,   k_far_plane};

            if (settings.radar)
                draw_radar_background(rect, settings.radar_range);

            if (settings.font)
                ImGui::PushFont(settings.font, settings.font_size);

            constexpr omath::Vector3 world_origin{0., 0., 0.};
            const auto origin_screen = camera.world_to_screen(world_origin);

            if (settings.origin_marker && origin_screen.has_value())
                draw_origin_marker({static_cast<float>(origin_screen->x), static_cast<float>(origin_screen->y)},
                                   state.position.distance_to(world_origin));

            if (settings.radar)
                draw_radar_blip(rect, camera, world_origin, settings.radar_range, IM_COL32(255, 170, 0, 255));

            auto drawn = 0;

            // One position read per zed feeds both the box and the radar blip
            for (const auto& zed : zeds)
            {
                const auto distance = state.position.distance_to(zed.position);

                if (settings.radar)
                    draw_radar_blip(rect, camera, zed.position, settings.radar_range, IM_COL32(80, 220, 120, 255));

                if (settings.zeds && distance <= settings.max_distance && zed.bounds.has_value()
                    && draw_actor_box(camera, zed, *zed.bounds, distance, settings.color, settings))
                    ++drawn;
            }

            for (const auto& ally : allies)
            {
                if (!settings.allies)
                    break;

                const auto distance = state.position.distance_to(ally.position);

                if (settings.radar)
                    draw_radar_blip(rect, camera, ally.position, settings.radar_range,
                                    ImGui::ColorConvertFloat4ToU32(settings.ally_color));

                if (distance <= settings.max_distance && ally.bounds.has_value())
                    draw_actor_box(camera, ally, *ally.bounds, distance, settings.ally_color, settings);
            }

            if (settings.font)
                ImGui::PopFont();

            if (settings.status_panel)
                draw_status(
                        std::format("kf1 overlay {}x{}   END quit\n"
                                    "camera  {:.0f} {:.0f} {:.0f}\n"
                                    "angles  pitch {:.1f}  yaw {:.1f}  roll {:.1f}\n"
                                    "fov     {:.1f}\n"
                                    "origin  {}\n"
                                    "zeds    {}{} tracked, {} on screen",
                                    rect.width, rect.height, state.position.x, state.position.y, state.position.z,
                                    state.angles.pitch.as_degrees(), state.angles.yaw.as_degrees(),
                                    state.angles.roll.as_degrees(), state.fov,
                                    origin_screen.has_value()
                                            ? std::format("{:.0f}, {:.0f} on screen", origin_screen->x, origin_screen->y)
                                            : "off screen",
                                    tracker.tracked().size(), tracker.capped() ? " (capped)" : "", drawn));
        }
        else if (settings.status_panel)
            draw_status("kf1 overlay\nno local player, waiting for a level");
    }
} // namespace kf1_overlay
