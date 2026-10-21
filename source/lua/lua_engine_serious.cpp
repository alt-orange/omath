//
// Created by Orange on 10/21/2026.
//
#ifdef OMATH_ENABLE_LUA
#include "lua_engines_detail.hpp"
#include <omath/engines/serious_engine/camera.hpp>

namespace omath::lua::detail
{
    void register_serious_engine(sol::table& omath_table)
    {
        register_engine<omath::serious_engine::Camera, omath::serious_engine::PitchAngle,
                        omath::serious_engine::ViewAngles>(omath_table, "serious");
    }
} // namespace omath::lua::detail
#endif
