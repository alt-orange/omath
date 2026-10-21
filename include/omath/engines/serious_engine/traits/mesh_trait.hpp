//
// Created by Orange on 10/21/2026.
//
#pragma once
#include <omath/engines/serious_engine/constants.hpp>
#include <omath/engines/serious_engine/formulas.hpp>

namespace omath::serious_engine
{
    class MeshTrait final
    {
    public:
        [[nodiscard("rotation matrix result should not be discarded")]]
        constexpr static Mat4X4 rotation_matrix(const ViewAngles& rotation) noexcept
        {
            return serious_engine::rotation_matrix(rotation);
        }
    };
} // namespace omath::serious_engine
