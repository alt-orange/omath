//
// Created by Orange on 10/21/2026.
//
#pragma once
#include "omath/engines/serious_engine/constants.hpp"
#include <utility>

namespace omath::serious_engine
{
    [[nodiscard("rotation matrix result should not be discarded")]]
    constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept;

    [[nodiscard("forward vector result should not be discarded")]]
    constexpr Vector3<float> forward_vector(const ViewAngles& angles) noexcept
    {
        return mat_rotate_vector(rotation_matrix(angles), k_abs_forward);
    }

    [[nodiscard("right vector result should not be discarded")]]
    constexpr Vector3<float> right_vector(const ViewAngles& angles) noexcept
    {
        return mat_rotate_vector(rotation_matrix(angles), k_abs_right);
    }

    [[nodiscard("up vector result should not be discarded")]]
    constexpr Vector3<float> up_vector(const ViewAngles& angles) noexcept
    {
        return mat_rotate_vector(rotation_matrix(angles), k_abs_up);
    }

    // Heading (Y) * pitch (X) * banking (Z), entry for entry the same as MakeRotationMatrix() in Serious Engine's
    // Sources/Engine/Math/Geometry.cpp. Banking acts around the camera's own forward axis.
    [[nodiscard("rotation matrix result should not be discarded")]]
    constexpr Mat4X4 rotation_matrix(const ViewAngles& angles) noexcept
    {
        return mat_rotation_yxz<float, MatStoreType::ROW_MAJOR>(angles.yaw, angles.pitch, angles.roll);
    }

    [[nodiscard("origin result should not be discarded")]]
    constexpr Vector3<float> extract_origin(const Mat4X4& mat) noexcept
    {
        return mat_extract_origin(mat);
    }

    [[nodiscard("scale result should not be discarded")]]
    constexpr Vector3<float> extract_scale(const Mat4X4& mat) noexcept
    {
        return mat_extract_scale(mat);
    }

    [[nodiscard("rotation angles result should not be discarded")]]
    constexpr ViewAngles extract_rotation_angles(const Mat4X4& mat) noexcept
    {
        // At pitch +/-90 heading and banking turn around the same axis; banking is then pinned to 0
        const auto angles = mat_extract_rotation_yxz(mat);
        return {
                PitchAngle::from_degrees(angles.x),
                YawAngle::from_degrees(angles.y),
                RollAngle::from_degrees(angles.z),
        };
    }

    [[nodiscard("view matrix result should not be discarded")]]
    constexpr Mat4X4 calc_view_matrix(const ViewAngles& angles, const Vector3<float>& cam_origin) noexcept
    {
        // Build the rotation once - calling forward/right/up_vector() separately would redo the matrix build three
        // times over. The rows are right, up and back (-forward), the same layout the engine keeps its view matrix in.
        const auto rotation = rotation_matrix(angles);

        return mat_camera_view(-mat_rotate_vector(rotation, k_abs_forward), mat_rotate_vector(rotation, k_abs_right),
                               mat_rotate_vector(rotation, k_abs_up), cam_origin);
    }

    // The engine's FOV is horizontal (ppr_FOVWidth in Sources/Engine/Math/Projection_Perspective.cpp): X scale is
    // 1 / tan(fov / 2) and the Y scale follows from the aspect ratio.
    [[nodiscard("perspective projection matrix result should not be discarded")]]
    constexpr Mat4X4
    calc_perspective_projection_matrix(const float field_of_view, const float aspect_ratio, const float near_plane,
                                       const float far_plane,
                                       const NDCDepthRange ndc_depth_range = NDCDepthRange::ZERO_TO_ONE) noexcept
    {
        if (ndc_depth_range == NDCDepthRange::ZERO_TO_ONE)
            return mat_perspective_right_handed_horizontal_fov<float, MatStoreType::ROW_MAJOR,
                                                               NDCDepthRange::ZERO_TO_ONE>(field_of_view, aspect_ratio,
                                                                                           near_plane, far_plane);
        if (ndc_depth_range == NDCDepthRange::NEGATIVE_ONE_TO_ONE)
            return mat_perspective_right_handed_horizontal_fov<float, MatStoreType::ROW_MAJOR,
                                                               NDCDepthRange::NEGATIVE_ONE_TO_ONE>(
                    field_of_view, aspect_ratio, near_plane, far_plane);
        std::unreachable();
    }

    template<class FloatingType>
    requires std::is_floating_point_v<FloatingType>
    [[nodiscard("centimeters value should not be discarded")]]
    constexpr FloatingType units_to_centimeters(const FloatingType& units)
    {
        constexpr auto centimeter_in_unit = static_cast<FloatingType>(100);
        return units * centimeter_in_unit;
    }

    template<class FloatingType>
    requires std::is_floating_point_v<FloatingType>
    [[nodiscard("meters value should not be discarded")]]
    constexpr FloatingType units_to_meters(const FloatingType& units)
    {
        return units;
    }

    template<class FloatingType>
    requires std::is_floating_point_v<FloatingType>
    [[nodiscard("kilometers value should not be discarded")]]
    constexpr FloatingType units_to_kilometers(const FloatingType& units)
    {
        return units_to_meters(units) / static_cast<FloatingType>(1000);
    }

    template<class FloatingType>
    requires std::is_floating_point_v<FloatingType>
    [[nodiscard("units value should not be discarded")]]
    constexpr FloatingType centimeters_to_units(const FloatingType& centimeters)
    {
        constexpr auto centimeter_in_unit = static_cast<FloatingType>(100);
        return centimeters / centimeter_in_unit;
    }

    template<class FloatingType>
    requires std::is_floating_point_v<FloatingType>
    [[nodiscard("units value should not be discarded")]]
    constexpr FloatingType meters_to_units(const FloatingType& meters)
    {
        return meters;
    }

    template<class FloatingType>
    requires std::is_floating_point_v<FloatingType>
    [[nodiscard("units value should not be discarded")]]
    constexpr FloatingType kilometers_to_units(const FloatingType& kilometers)
    {
        return meters_to_units(kilometers * static_cast<FloatingType>(1000));
    }
} // namespace omath::serious_engine
