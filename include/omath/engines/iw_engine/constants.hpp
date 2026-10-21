//
// Created by Vlad on 3/17/2025.
//

#pragma once
#include "omath/linear_algebra/mat.hpp"
#include "omath/linear_algebra/vector3.hpp"
#include <omath/trigonometry/angle.hpp>
#include <omath/trigonometry/view_angles.hpp>

namespace omath::iw_engine
{
    constexpr Vector3<float> k_abs_up = {0, 0, 1};
    constexpr Vector3<float> k_abs_right = {0, -1, 0};
    constexpr Vector3<float> k_abs_forward = {1, 0, 0};

    // The FOV is horizontal at this aspect ratio, whatever the actual viewport is
    constexpr float k_fov_reference_aspect_ratio = 4.f / 3.f;

    using Mat4X4 = Mat<4, 4, float, MatStoreType::ROW_MAJOR>;
    using Mat3X3 = Mat<3, 3, float, MatStoreType::ROW_MAJOR>;
    using Mat1X3 = Mat<1, 3, float, MatStoreType::ROW_MAJOR>;
    using PitchAngle = Angle<float, -89.f, 89.f, AngleFlags::Clamped>;
    using YawAngle = Angle<float, -180.f, 180.f, AngleFlags::Normalized>;
    using RollAngle = Angle<float, -180.f, 180.f, AngleFlags::Normalized>;

    using ViewAngles = omath::ViewAngles<PitchAngle, YawAngle, RollAngle>;
} // namespace omath::iw_engine