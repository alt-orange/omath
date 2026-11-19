//
// Created by Vlad on 8/14/2026.
//
// Click through overlay for Killing Floor 1 (KF_VERSION 1065, Unreal Engine 2.5).
//
// A transparent, always on top GLFW window is pinned to the game's client area and ImGui draws into it. The camera is
// read out of the game with ReadProcessMemory (see kf1_common/kf1_target.hpp), handed to omath's Unreal camera, and
// both the world origin {0, 0, 0} and every living zed are projected with world_to_screen. Zeds are found on their own,
// anything derived from KFMonster counts.
//
// END closes the overlay. The overlay never takes focus, because UE 2.5 minimizes a fullscreen game as soon as it
// loses focus. END is ignored unless the game is in front, which keeps one pressed in some other application from
// closing the overlay.
//
// Nothing is injected and nothing is written to the game, this is a read only observer.

#include "kf1_common/kf1_target.hpp"
#include "kf1_overlay_common.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <print>
#include <thread>

#define GLFW_EXPOSE_NATIVE_WIN32
#include "omath/hud/hud_renderer_interface.hpp"
#include "omath/hud/screen_overlay.hpp"
#include <GLFW/glfw3native.h>

using namespace kf1_overlay;

namespace
{
    // Where the game draws, in screen coordinates - that is exactly the area the overlay has to cover for a
    // projection made with the game's own viewport to line up
    [[nodiscard]]
    WindowRect client_rect_on_screen(const HWND window)
    {
        RECT client{};
        POINT top_left{};

        if (!GetClientRect(window, &client) || !ClientToScreen(window, &top_left))
            return {};

        return {top_left.x, top_left.y, client.right - client.left, client.bottom - client.top};
    }

    [[nodiscard]]
    GLFWwindow* create_overlay_window(const WindowRect& rect)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        auto* window = glfwCreateWindow(rect.width, rect.height, "omath kf1 overlay", nullptr, nullptr);

        if (!window)
            return nullptr;

        glfwSetWindowPos(window, rect.x, rect.y);
        glfwShowWindow(window);

        return window;
    }

    // Every click goes to the game underneath. WS_EX_NOACTIVATE and the fact that the overlay never calls
    // SetForegroundWindow keep the game in front: UE 2.5 minimizes a fullscreen game the moment it loses focus, which
    // would take the game and the overlay with it off the screen.
    void set_click_through(GLFWwindow* window)
    {
        glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);

        const auto native = glfwGetWin32Window(window);
        const auto style =
                GetWindowLongPtrW(native, GWL_EXSTYLE) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;

        SetWindowLongPtrW(native, GWL_EXSTYLE, style);
    }
} // namespace

int main()
{
    const auto target = kf1::attach();

    if (!target.has_value())
        return -1;

    const auto game_window = kf1::find_main_window(target->process_id);

    if (!game_window)
    {
        std::println("[ERROR] KillingFloor.exe has no visible window");
        return -1;
    }

    std::println("resolving reflection classes, this walks every object name once...");

    const auto classes = kf1::find_reflection_classes(*target);

    if (!classes.complete())
    {
        std::println("[ERROR] no Actor/PlayerController/Pawn/Property class, get past the main menu and start the "
                     "overlay again");
        return -1;
    }

    auto player = kf1::find_local_player(*target, classes);

    if (!glfwInit())
    {
        std::println("[ERROR] glfwInit failed");
        return -1;
    }

    auto overlay_rect = client_rect_on_screen(game_window);

    // A minimized game has no client area to match, start full screen sized and let the follow logic below fix it up
    if (overlay_rect.width <= 0 || overlay_rect.height <= 0)
        overlay_rect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};

    auto* window = create_overlay_window(overlay_rect);

    if (!window)
    {
        std::println("[ERROR] failed to create the overlay window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    set_click_through(window);

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // No imgui.ini next to the binary, there is no window whose layout would be worth keeping
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    std::println("overlay is up, END closes it while the game is in front");

    ActorTracker tracker;
    std::chrono::steady_clock::time_point last_player_search;
    auto end_was_down = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // GetAsyncKeyState is global, so the edge is always consumed, but it only counts while the game is in front -
        // otherwise END typed in another application would close the overlay
        if (key_pressed(VK_END, end_was_down) && GetForegroundWindow() == game_window)
            break;

        // Follow the game window, it can be moved, resized or switched between windowed and fullscreen
        if (const auto rect = client_rect_on_screen(game_window); rect != overlay_rect && rect.width > 0)
        {
            overlay_rect = rect;
            glfwSetWindowPos(window, rect.x, rect.y);
            glfwSetWindowSize(window, rect.width, rect.height);
        }

        // A minimized game has nothing to draw over
        if (IsIconic(game_window))
        {
            glfwHideWindow(window);
            // Nothing gets swapped while hidden, so idle deliberately instead of spinning on vsync
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        glfwShowWindow(window);

        // Cheap validation every frame, the expensive lookup only happens once the player is gone or has changed,
        // which is what a level change or a respawn looks like from out here
        refresh_local_player(*target, classes, player, last_player_search);

        tracker.advance(*target, classes);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_overlay(overlay_rect, player, tracker, read_living_zeds(tracker, player),
                     read_living_allies(tracker, player), EspSettings{});

        ImGui::Render();

        int framebuffer_width{};
        int framebuffer_height{};
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    CloseHandle(kf1::g_process);

    return 0;
}
