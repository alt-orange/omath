//
// Created by orange on 13.03.2026.
//
//
// Created by Vlad on 6/17/2025.
//
#include "omath/hud/canvas_box.hpp"

namespace omath::hud
{

    CanvasBox::CanvasBox(const Vector2<float> top, Vector2<float> bottom, const float ratio)
    {
        bottom.x = top.x;
        const auto height = std::abs(top.y - bottom.y);

        top_left_corner = top - Vector2<float>{height / ratio, 0};
        top_right_corner = top + Vector2<float>{height / ratio, 0};

        bottom_left_corner = bottom - Vector2<float>{height / ratio, 0};
        bottom_right_corner = bottom + Vector2<float>{height / ratio, 0};
    }
    CanvasBox CanvasBox::from_bounds(const Vector2<float>& min, const Vector2<float>& max)
    {
        CanvasBox box;
        box.top_left_corner = {min.x, min.y};
        box.top_right_corner = {max.x, min.y};
        box.bottom_left_corner = {min.x, max.y};
        box.bottom_right_corner = {max.x, max.y};
        return box;
    }

    std::array<Vector2<float>, 4> CanvasBox::as_array() const
    {
        return {top_left_corner, top_right_corner, bottom_right_corner, bottom_left_corner};
    }
} // namespace ohud