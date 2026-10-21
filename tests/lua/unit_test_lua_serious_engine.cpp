//
// Created by Orange on 10/21/2026.
//
#include <gtest/gtest.h>
#include <lua.hpp>
#include <omath/lua/lua.hpp>

class LuaSeriousEngine : public ::testing::Test
{
protected:
    lua_State* L = nullptr;

    void SetUp() override
    {
        L = luaL_newstate();
        luaL_openlibs(L);
        omath::lua::LuaInterpreter::register_lib(L);
        if (luaL_dofile(L, LUA_SCRIPTS_DIR "/serious_engine_tests.lua") != LUA_OK)
            FAIL() << lua_tostring(L, -1);
    }

    void TearDown() override { lua_close(L); }

    void check(const char* func_name)
    {
        lua_getglobal(L, func_name);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            FAIL() << lua_tostring(L, -1);
            lua_pop(L, 1);
        }
    }
};

TEST_F(LuaSeriousEngine, PitchAngle_clamps_to_90)                    { check("Serious_PitchAngle_clamps_to_90"); }
TEST_F(LuaSeriousEngine, YawAngle_normalization)                     { check("Serious_YawAngle_normalization"); }
TEST_F(LuaSeriousEngine, Camera_default_looks_down_negative_z)       { check("Serious_Camera_default_looks_down_negative_z"); }
TEST_F(LuaSeriousEngine, Camera_positive_heading_turns_left)         { check("Serious_Camera_positive_heading_turns_left"); }
TEST_F(LuaSeriousEngine, Camera_horizontal_fov_reaches_the_screen_edge) { check("Serious_Camera_horizontal_fov_reaches_the_screen_edge"); }
TEST_F(LuaSeriousEngine, Camera_extract_projection_params)           { check("Serious_Camera_extract_projection_params"); }
TEST_F(LuaSeriousEngine, Camera_calc_view_angles_from_view_matrix)   { check("Serious_Camera_calc_view_angles_from_view_matrix"); }
TEST_F(LuaSeriousEngine, Camera_calc_origin_from_view_matrix)        { check("Serious_Camera_calc_origin_from_view_matrix"); }
TEST_F(LuaSeriousEngine, Camera_world_to_screen_center)              { check("Serious_Camera_world_to_screen_center"); }
TEST_F(LuaSeriousEngine, Camera_world_to_screen_behind_is_error)     { check("Serious_Camera_world_to_screen_behind_is_error"); }
TEST_F(LuaSeriousEngine, Camera_look_at_centers_the_target)          { check("Serious_Camera_look_at_centers_the_target"); }
TEST_F(LuaSeriousEngine, Camera_screen_to_world)                     { check("Serious_Camera_screen_to_world"); }
