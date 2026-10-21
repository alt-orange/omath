//
// Created by Orange on 10/21/2026.
//
#pragma once
#include "omath/engines/serious_engine/constants.hpp"
#include "omath/projection/camera.hpp"
#include "traits/camera_trait.hpp"

namespace omath::serious_engine
{
    using Camera =
            projection::Camera<Mat4X4, ViewAngles, CameraTrait, NDCDepthRange::ZERO_TO_ONE, {.inverted_forward = true}>;
} // namespace omath::serious_engine
