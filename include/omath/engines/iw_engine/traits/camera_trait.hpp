//
// Created by Vlad on 8/10/2025.
//

#pragma once
#include "omath/engines/iw_engine/formulas.hpp"
#include "omath/projection/camera.hpp"

namespace omath::iw_engine
{
    class CameraTrait final
    {
    public:
        [[nodiscard("look-at angle result should not be discarded")]]
        constexpr static ViewAngles calc_look_at_angle(const Vector3<float>& cam_origin,
                                                       const Vector3<float>& look_at) noexcept
        {
            const auto direction = (look_at - cam_origin).normalized();

            return {-PitchAngle::from_asin(direction.z), YawAngle::from_atan2(direction.y, direction.x),
                    RollAngle::from_radians(0.f)};
        }

        [[nodiscard("view matrix result should not be discarded")]]
        constexpr static Mat4X4 calc_view_matrix(const ViewAngles& angles, const Vector3<float>& cam_origin) noexcept
        {
            return iw_engine::calc_view_matrix(angles, cam_origin);
        }
        // Inverse of rotation_matrix(); lets Camera::calc_view_angles_from_view_matrix() recover all three angles
        [[nodiscard("rotation angles result should not be discarded")]]
        constexpr static ViewAngles extract_rotation_angles(const Mat4X4& rotation) noexcept
        {
            return iw_engine::extract_rotation_angles(rotation);
        }
        [[nodiscard("projection matrix result should not be discarded")]]
        constexpr static Mat4X4 calc_projection_matrix(const projection::FieldOfView& fov,
                                                       const projection::ViewPort& view_port, const float near_plane,
                                                       const float far_plane,
                                                       const NDCDepthRange ndc_depth_range) noexcept
        {
            return calc_perspective_projection_matrix(fov.as_degrees(), view_port.aspect_ratio(), near_plane, far_plane,
                                                      ndc_depth_range);
        }
        // The FOV is horizontal at a 4:3 reference aspect ratio (see calc_perspective_projection_matrix), so recover
        // the vertical FOV from m[1,1] and widen it back to 4:3.
        [[nodiscard("field of view result should not be discarded")]]
        constexpr static projection::FieldOfView calc_field_of_view(const Mat4X4& projection_matrix) noexcept
        {
            const auto vertical_fov =
                    angles::radians_to_degrees(2.f * internal::atan(1.f / projection_matrix.at(1, 1)));
            return projection::FieldOfView::from_degrees(
                    angles::vertical_fov_to_horizontal(vertical_fov, k_fov_reference_aspect_ratio));
        }
    };

} // namespace omath::iw_engine
