//
// Created by Orange on 10/21/2026.
//
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <omath/engines/serious_engine/camera.hpp>
#include <omath/engines/serious_engine/collider.hpp>
#include <omath/engines/serious_engine/constants.hpp>
#include <omath/engines/serious_engine/formulas.hpp>
#include <omath/engines/serious_engine/primitives.hpp>
#include <omath/engines/serious_engine/traits/mesh_trait.hpp>
#include <omath/engines/serious_engine/traits/pred_engine_trait.hpp>
#include <omath/projectile_prediction/projectile.hpp>
#include <ranges>
#include <type_traits>

using namespace omath;

static_assert(std::is_same_v<serious_engine::Mat4X4::ContainedType, float>);

namespace
{
    constexpr float k_pi = std::numbers::pi_v<float>;

    serious_engine::ViewAngles make_angles(const float pitch, const float yaw, const float roll = 0.f)
    {
        return {serious_engine::PitchAngle::from_degrees(pitch), serious_engine::YawAngle::from_degrees(yaw),
                serious_engine::RollAngle::from_degrees(roll)};
    }

    void expect_vector_near(const Vector3<float>& actual, const Vector3<float>& expected, const float tolerance)
    {
        for (const auto& [result, etalon] : std::views::zip(actual.as_array(), expected.as_array()))
            EXPECT_NEAR(result, etalon, tolerance);
    }

    float angle_difference(const float a, const float b)
    {
        return std::remainder(a - b, 360.f);
    }
} // namespace

// ---------------------------------------------------------------- conventions

TEST(unit_test_serious_engine, DefaultAxes)
{
    EXPECT_EQ(serious_engine::forward_vector({}), serious_engine::k_abs_forward);
    EXPECT_EQ(serious_engine::right_vector({}), serious_engine::k_abs_right);
    EXPECT_EQ(serious_engine::up_vector({}), serious_engine::k_abs_up);
}

TEST(unit_test_serious_engine, AxesAreRightHandedYUpLookingDownNegativeZ)
{
    EXPECT_EQ(serious_engine::k_abs_forward, (Vector3<float>{0.f, 0.f, -1.f}));
    EXPECT_EQ(serious_engine::k_abs_right, (Vector3<float>{1.f, 0.f, 0.f}));
    EXPECT_EQ(serious_engine::k_abs_up, (Vector3<float>{0.f, 1.f, 0.f}));
    // right x up points backwards in a right-handed system that looks down -Z
    expect_vector_near(serious_engine::k_abs_right.cross(serious_engine::k_abs_up), -serious_engine::k_abs_forward,
                       1e-6f);
}

TEST(unit_test_serious_engine, PositiveHeadingTurnsLeft)
{
    const auto angles = make_angles(0.f, 90.f);

    expect_vector_near(serious_engine::forward_vector(angles), {-1.f, 0.f, 0.f}, 1e-5f);
    expect_vector_near(serious_engine::right_vector(angles), {0.f, 0.f, -1.f}, 1e-5f);
    expect_vector_near(serious_engine::up_vector(angles), {0.f, 1.f, 0.f}, 1e-5f);
}

TEST(unit_test_serious_engine, PositivePitchLooksUp)
{
    const auto forward = serious_engine::forward_vector(make_angles(30.f, 0.f));

    EXPECT_NEAR(forward.y, 0.5f, 1e-5f);
    EXPECT_NEAR(forward.z, -std::cos(k_pi / 6.f), 1e-5f);
}

// Camera vectors read from a running Serious Sam HD: The Second Encounter next to the angles it reported.
TEST(unit_test_serious_engine, MatchesAxesMeasuredInTheGame_Sample1)
{
    // yaw 14.8, pitch -2.5
    const auto angles = make_angles(-2.5f, 14.8f);

    expect_vector_near(serious_engine::forward_vector(angles), {-0.254f, -0.044f, -0.966f}, 2e-3f);
    expect_vector_near(serious_engine::right_vector(angles), {0.967f, -0.002f, -0.254f}, 2e-3f);
    expect_vector_near(serious_engine::up_vector(angles), {-0.010f, 0.999f, -0.044f}, 2e-3f);
}

TEST(unit_test_serious_engine, MatchesAxesMeasuredInTheGame_Sample2)
{
    // yaw 90.3, pitch -0.7: looking almost exactly down -X
    const auto angles = make_angles(-0.7f, 90.3f);

    expect_vector_near(serious_engine::forward_vector(angles), {-1.000f, -0.012f, 0.005f}, 2e-3f);
    expect_vector_near(serious_engine::right_vector(angles), {-0.004f, 0.000f, -1.000f}, 2e-3f);
    expect_vector_near(serious_engine::up_vector(angles), {-0.012f, 1.000f, 0.000f}, 2e-3f);
}

// ---------------------------------------------------------------- rotation

// Reference values from MakeRotationMatrix() in Serious Engine's Sources/Engine/Math/Geometry.cpp, evaluated in double
// precision outside omath. Row i, column j here is t3dRotation(i + 1, j + 1) there.
TEST(unit_test_serious_engine, RotationMatrixMatchesTheEnginesMakeRotationMatrix)
{
    struct Sample
    {
        float heading, pitch, banking;
        float expected[3][3];
    };
    constexpr Sample samples[] = {
            {30.f,
             20.f,
             10.f,
             {{0.882564f, 0.018028f, 0.469846f},
              {0.163176f, 0.925417f, -0.342020f},
              {-0.440970f, 0.378522f, 0.813798f}}},
            {-120.f,
             -45.f,
             75.f,
             {{0.462097f, 0.641457f, -0.612372f},
              {0.683013f, 0.183013f, 0.707107f},
              {0.565650f, -0.745010f, -0.353553f}}},
            {170.f,
             60.f,
             -140.f,
             {{0.657742f, -0.748223f, 0.086824f},
              {-0.321394f, -0.383022f, -0.866025f},
              {0.681236f, 0.541716f, -0.492404f}}},
    };

    for (const auto& [heading, pitch, banking, expected] : samples)
    {
        const auto actual = serious_engine::rotation_matrix(make_angles(pitch, heading, banking));

        for (std::size_t row = 0; row < 3; row++)
            for (std::size_t col = 0; col < 3; col++)
                EXPECT_NEAR(actual.at(row, col), expected[row][col], 1e-5f)
                        << "heading " << heading << " pitch " << pitch << " banking " << banking << " at " << row
                        << ", " << col;
    }
}

TEST(unit_test_serious_engine, RotationMatrixIsHeadingThenPitchThenBanking)
{
    // The same product written with omath's axis rotations: Ry(heading) * Rx(pitch) * Rz(banking).
    for (const auto pitch : {-80.f, -30.f, 0.f, 25.f, 70.f})
        for (const auto yaw : {-170.f, -60.f, 0.f, 45.f, 135.f})
            for (const auto roll : {-120.f, -15.f, 0.f, 30.f, 150.f})
            {
                const auto angles = make_angles(pitch, yaw, roll);
                const auto expected = mat_rotation_axis_y(angles.yaw) * mat_rotation_axis_x(angles.pitch)
                                      * mat_rotation_axis_z(angles.roll);
                const auto actual = serious_engine::rotation_matrix(angles);

                for (std::size_t row = 0; row < 4; row++)
                    for (std::size_t col = 0; col < 4; col++)
                        EXPECT_NEAR(actual.at(row, col), expected.at(row, col), 1e-5f)
                                << "pitch " << pitch << " yaw " << yaw << " roll " << roll;
            }
}

TEST(unit_test_serious_engine, RollRotatesAroundTheForwardAxis)
{
    const auto angles = make_angles(40.f, 25.f, 33.f);
    const auto level = make_angles(40.f, 25.f, 0.f);

    // banking must not move where the camera looks, only turn right/up around it
    expect_vector_near(serious_engine::forward_vector(angles), serious_engine::forward_vector(level), 1e-5f);
    EXPECT_NEAR(serious_engine::right_vector(angles).dot(serious_engine::right_vector(level)),
                std::cos(33.f * k_pi / 180.f), 1e-5f);
}

TEST(unit_test_serious_engine, ExtractRotationAnglesRoundTrip)
{
    for (const auto pitch : {-80.f, -30.f, 0.f, 25.f, 70.f})
        for (const auto yaw : {-170.f, -60.f, 0.f, 45.f, 135.f})
            for (const auto roll : {-120.f, -15.f, 0.f, 30.f, 150.f})
            {
                const auto angles = make_angles(pitch, yaw, roll);
                const auto extracted = serious_engine::extract_rotation_angles(serious_engine::rotation_matrix(angles));

                EXPECT_NEAR(extracted.pitch.as_degrees(), pitch, 1e-3f);
                EXPECT_NEAR(angle_difference(extracted.yaw.as_degrees(), yaw), 0.f, 1e-3f);
                EXPECT_NEAR(angle_difference(extracted.roll.as_degrees(), roll), 0.f, 1e-3f);
            }
}

TEST(unit_test_serious_engine, ExtractRotationAnglesAtStraightUpAndDownKeepsTheDirection)
{
    // heading and banking are the same rotation at +/-90 pitch, so only the resulting orientation is recoverable
    for (const auto pitch : {-90.f, 90.f})
    {
        const auto angles = make_angles(pitch, 40.f, 0.f);
        const auto extracted = serious_engine::extract_rotation_angles(serious_engine::rotation_matrix(angles));

        EXPECT_NEAR(extracted.pitch.as_degrees(), pitch, 1e-3f);
        expect_vector_near(serious_engine::forward_vector(extracted), serious_engine::forward_vector(angles), 1e-4f);
        expect_vector_near(serious_engine::right_vector(extracted), serious_engine::right_vector(angles), 1e-4f);
    }
}

TEST(unit_test_serious_engine, ExtractOriginAndScale)
{
    auto mat = serious_engine::rotation_matrix(make_angles(10.f, 20.f, 5.f));
    mat.at(0, 3) = 11.f;
    mat.at(1, 3) = -22.f;
    mat.at(2, 3) = 33.f;

    expect_vector_near(serious_engine::extract_origin(mat), {11.f, -22.f, 33.f}, 1e-5f);
    expect_vector_near(serious_engine::extract_scale(mat), {1.f, 1.f, 1.f}, 1e-5f);
}

// ---------------------------------------------------------------- view matrix

// A full 3x4 view matrix read from the engine's own camera globals, with the eye position it was built from.
// Rows are right|tx, up|ty, back|tz (back is -forward). The game reported the angles rounded to 0.1 degree
// (pitch -14.6, heading 144.6); k_game_pitch / k_game_heading are the full-precision values solved from the back and
// right rows, so the up row and the whole translation column are independent checks.
namespace
{
    constexpr float k_game_pitch = -14.6492f;
    constexpr float k_game_heading = 144.5696f;
    constexpr Vector3<float> k_game_eye = {-17.1773f, -22.0306f, -520.0123f};
} // namespace

TEST(unit_test_serious_engine, ViewMatrixMatchesTheEnginesOwn)
{
    const auto view = serious_engine::calc_view_matrix(make_angles(k_game_pitch, k_game_heading), k_game_eye);

    constexpr float expected[3][4] = {
            {-0.8148f, 0.0f, -0.5797f, -315.4465f},
            {-0.1466f, 0.9675f, 0.2061f, 125.9520f},
            {0.5609f, 0.2529f, -0.7883f, -394.7441f},
    };
    // the captured entries are printed with 4 decimals
    for (std::size_t row = 0; row < 3; row++)
        for (std::size_t col = 0; col < 3; col++)
            EXPECT_NEAR(view.at(row, col), expected[row][col], 2e-4f) << "row " << row << " col " << col;

    // the translations are ~500 units, so float rounding alone is ~0.01
    for (std::size_t row = 0; row < 3; row++)
        EXPECT_NEAR(view.at(row, 3), expected[row][3], 0.05f) << "row " << row;
    EXPECT_FLOAT_EQ(view.at(3, 3), 1.f);
}

TEST(unit_test_serious_engine, ViewMatrixMapsTheEyeToTheOrigin)
{
    const auto angles = make_angles(23.f, -71.f, 12.f);
    const Vector3<float> eye = {120.f, -34.f, 560.f};
    const auto view = serious_engine::calc_view_matrix(angles, eye);

    const auto in_view_space = [&](const Vector3<float>& point)
    {
        return Vector3<float>{
                view.at(0, 0) * point.x + view.at(0, 1) * point.y + view.at(0, 2) * point.z + view.at(0, 3),
                view.at(1, 0) * point.x + view.at(1, 1) * point.y + view.at(1, 2) * point.z + view.at(1, 3),
                view.at(2, 0) * point.x + view.at(2, 1) * point.y + view.at(2, 2) * point.z + view.at(2, 3)};
    };

    expect_vector_near(in_view_space(eye), {0.f, 0.f, 0.f}, 1e-3f);
    // one unit in front of the eye is one unit down the view-space -Z axis
    expect_vector_near(in_view_space(eye + serious_engine::forward_vector(angles)), {0.f, 0.f, -1.f}, 1e-4f);
    expect_vector_near(in_view_space(eye + serious_engine::right_vector(angles)), {1.f, 0.f, 0.f}, 1e-4f);
    expect_vector_near(in_view_space(eye + serious_engine::up_vector(angles)), {0.f, 1.f, 0.f}, 1e-4f);
}

TEST(unit_test_serious_engine, CameraRecoversOriginAndAnglesFromItsViewMatrix)
{
    const auto angles = make_angles(-12.f, 118.f);
    const Vector3<float> eye = {-300.f, 75.f, 410.f};
    const auto view = serious_engine::calc_view_matrix(angles, eye);

    expect_vector_near(serious_engine::Camera::calc_origin_from_view_matrix(view), eye, 1e-3f);

    const auto recovered = serious_engine::Camera::calc_view_angles_from_view_matrix(view);
    EXPECT_NEAR(recovered.pitch.as_degrees(), -12.f, 1e-3f);
    EXPECT_NEAR(angle_difference(recovered.yaw.as_degrees(), 118.f), 0.f, 1e-3f);
}

// ---------------------------------------------------------------- projection

TEST(unit_test_serious_engine, ProjectionUsesHorizontalFov)
{
    const auto proj = serious_engine::calc_perspective_projection_matrix(90.f, 1.25f, 0.2f, 1000.f);

    // x scale is 1 / tan(hfov / 2), y scale follows from the aspect ratio - the same ratio the engine's own matrix has
    EXPECT_NEAR(proj.at(0, 0), 1.f, 1e-5f);
    EXPECT_NEAR(proj.at(1, 1), 1.25f, 1e-5f);
    EXPECT_NEAR(proj.at(1, 1) / proj.at(0, 0), 1.25f, 1e-5f);
    // clip.w = -z_view
    EXPECT_FLOAT_EQ(proj.at(3, 2), -1.f);
    EXPECT_FLOAT_EQ(proj.at(3, 3), 0.f);
}

TEST(unit_test_serious_engine, ProjectionFovStaysHorizontalWhenTheAspectChanges)
{
    const auto narrow = serious_engine::calc_perspective_projection_matrix(75.f, 4.f / 3.f, 0.1f, 500.f);
    const auto wide = serious_engine::calc_perspective_projection_matrix(75.f, 16.f / 9.f, 0.1f, 500.f);

    EXPECT_NEAR(narrow.at(0, 0), wide.at(0, 0), 1e-6f);
    EXPECT_GT(wide.at(1, 1), narrow.at(1, 1));
}

TEST(unit_test_serious_engine, DepthRangeZeroToOneMapsNearToZeroAndFarToOne)
{
    constexpr float near_plane = 0.2f;
    constexpr float far_plane = 1000.f;
    const auto proj = serious_engine::calc_perspective_projection_matrix(90.f, 1.f, near_plane, far_plane,
                                                                         NDCDepthRange::ZERO_TO_ONE);
    const auto ndc_depth = [&](const float view_z)
    {
        return (proj.at(2, 2) * view_z + proj.at(2, 3)) / (proj.at(3, 2) * view_z);
    };

    EXPECT_NEAR(ndc_depth(-near_plane), 0.f, 1e-5f);
    EXPECT_NEAR(ndc_depth(-far_plane), 1.f, 1e-5f);
}

TEST(unit_test_serious_engine, DepthRangeNegativeOneToOneMapsNearToMinusOneAndFarToOne)
{
    constexpr float near_plane = 0.2f;
    constexpr float far_plane = 1000.f;
    const auto proj = serious_engine::calc_perspective_projection_matrix(90.f, 1.f, near_plane, far_plane,
                                                                         NDCDepthRange::NEGATIVE_ONE_TO_ONE);
    const auto ndc_depth = [&](const float view_z)
    {
        return (proj.at(2, 2) * view_z + proj.at(2, 3)) / (proj.at(3, 2) * view_z);
    };

    EXPECT_NEAR(ndc_depth(-near_plane), -1.f, 1e-4f);
    EXPECT_NEAR(ndc_depth(-far_plane), 1.f, 1e-4f);
}

// ---------------------------------------------------------------- camera

TEST(unit_test_serious_engine, WorldToScreenCenterForPointStraightAhead)
{
    const serious_engine::Camera cam({0, 0, 0}, {}, {1280.f, 1024.f}, projection::FieldOfView::from_degrees(90.f), 0.2f,
                                     1000.f);

    const auto projected = cam.world_to_screen({0.f, 0.f, -10.f});

    ASSERT_TRUE(projected.has_value());
    EXPECT_NEAR(projected->x, 640.f, 1e-3f);
    EXPECT_NEAR(projected->y, 512.f, 1e-3f);
}

TEST(unit_test_serious_engine, WorldToScreenHalfFovLandsOnTheScreenEdge)
{
    const serious_engine::Camera cam({0, 0, 0}, {}, {1280.f, 1024.f}, projection::FieldOfView::from_degrees(90.f), 0.2f,
                                     1000.f);

    // 90 degrees horizontal fov: 10 units right at 10 units ahead is exactly the right edge, on the middle row
    const auto right_edge = cam.world_to_screen_unclipped({10.f, 0.f, -10.f});
    ASSERT_TRUE(right_edge.has_value());
    EXPECT_NEAR(right_edge->x, 1280.f, 1e-2f);
    EXPECT_NEAR(right_edge->y, 512.f, 1e-2f);

    // the vertical half-extent follows the aspect ratio: 10 / 1.25 = 8 units up reaches the top edge
    const auto top_edge = cam.world_to_screen_unclipped({0.f, 8.f, -10.f});
    ASSERT_TRUE(top_edge.has_value());
    EXPECT_NEAR(top_edge->x, 640.f, 1e-2f);
    EXPECT_NEAR(top_edge->y, 0.f, 1e-2f);
}

TEST(unit_test_serious_engine, WorldToScreenRightIsRightAndUpIsUp)
{
    const serious_engine::Camera cam({0, 0, 0}, {}, {1280.f, 1024.f}, projection::FieldOfView::from_degrees(90.f), 0.2f,
                                     1000.f);

    const auto right = cam.world_to_screen({3.f, 0.f, -10.f});
    const auto up = cam.world_to_screen({0.f, 3.f, -10.f});

    ASSERT_TRUE(right.has_value());
    ASSERT_TRUE(up.has_value());
    EXPECT_GT(right->x, 640.f);
    EXPECT_LT(up->y, 512.f); // screen Y grows downwards from the top-left corner
}

TEST(unit_test_serious_engine, WorldToScreenRejectsPointsBehindTheCamera)
{
    const serious_engine::Camera cam({0, 0, 0}, {}, {1280.f, 1024.f}, projection::FieldOfView::from_degrees(90.f), 0.2f,
                                     1000.f);

    EXPECT_FALSE(cam.world_to_screen({0.f, 0.f, 10.f}).has_value());
}

TEST(unit_test_serious_engine, WorldToScreenFollowsHeading)
{
    // heading +90 turns left, so the camera looks down -X and a point there is dead centre
    const serious_engine::Camera cam({0, 0, 0}, make_angles(0.f, 90.f), {1280.f, 1024.f},
                                     projection::FieldOfView::from_degrees(90.f), 0.2f, 1000.f);

    const auto ahead = cam.world_to_screen({-10.f, 0.f, 0.f});
    ASSERT_TRUE(ahead.has_value());
    EXPECT_NEAR(ahead->x, 640.f, 1e-2f);
    EXPECT_NEAR(ahead->y, 512.f, 1e-2f);

    // the old forward direction (-Z) is now on the camera's right (a point exactly 90 degrees off-axis has no depth)
    const auto to_the_right = cam.world_to_screen({-10.f, 0.f, -5.f});
    ASSERT_TRUE(to_the_right.has_value());
    EXPECT_GT(to_the_right->x, 640.f);
}

TEST(unit_test_serious_engine, WorldToScreenMatchesTheEnginesOwnMatrices)
{
    // Same capture as ViewMatrixMatchesTheEnginesOwn, with a 5:4 viewport and projection scales sx = 0.9959,
    // sy = 1.2448. Pushing the world origin through the engine's own view rows gives:
    //   view = (-315.4465 * sx, 125.9520 * sy, w = 394.7441)  ->  ndc = (-0.79584, 0.39718)
    //   screen (1280 x 1024, origin top-left) = (130.66, 308.64)
    const float hfov = 2.f * std::atan(1.f / 0.9959f) * 180.f / k_pi;
    const serious_engine::Camera cam(k_game_eye, make_angles(k_game_pitch, k_game_heading), {1280.f, 1024.f},
                                     projection::FieldOfView::from_degrees(hfov), 0.2f, 100000.f);

    const auto projected = cam.world_to_screen({0.f, 0.f, 0.f});

    ASSERT_TRUE(projected.has_value());
    EXPECT_NEAR(projected->x, 130.66f, 0.5f);
    EXPECT_NEAR(projected->y, 308.64f, 0.5f);
}

// ---------------------------------------------------------------- traits

TEST(unit_test_serious_engine, LookAtForward)
{
    const auto angles = serious_engine::CameraTrait::calc_look_at_angle({}, serious_engine::k_abs_forward);

    expect_vector_near(serious_engine::forward_vector(angles), serious_engine::k_abs_forward, 1e-4f);
}

TEST(unit_test_serious_engine, LookAtRight)
{
    const auto angles = serious_engine::CameraTrait::calc_look_at_angle({}, serious_engine::k_abs_right);

    expect_vector_near(serious_engine::forward_vector(angles), serious_engine::k_abs_right, 1e-4f);
}

TEST(unit_test_serious_engine, LookAtUp)
{
    const auto angles = serious_engine::CameraTrait::calc_look_at_angle({}, serious_engine::k_abs_up);

    expect_vector_near(serious_engine::forward_vector(angles), serious_engine::k_abs_up, 1e-4f);
}

TEST(unit_test_serious_engine, LookAtArbitraryDirectionsPointsTheForwardVectorThere)
{
    for (const auto& target : {Vector3<float>{30.f, 10.f, -40.f}, Vector3<float>{-25.f, -15.f, 5.f},
                               Vector3<float>{-3.f, 20.f, 18.f}, Vector3<float>{44.f, -7.f, 12.f}})
    {
        const auto angles = serious_engine::CameraTrait::calc_look_at_angle({}, target);

        expect_vector_near(serious_engine::forward_vector(angles), target.normalized(), 1e-4f);
    }
}

TEST(unit_test_serious_engine, LookAtPutsTheTargetInTheMiddleOfTheScreen)
{
    serious_engine::Camera cam({10.f, 5.f, -20.f}, {}, {1280.f, 1024.f}, projection::FieldOfView::from_degrees(90.f),
                               0.2f, 1000.f);
    constexpr Vector3<float> target = {-40.f, 30.f, -90.f};
    cam.look_at(target);

    const auto projected = cam.world_to_screen(target);
    ASSERT_TRUE(projected.has_value());
    EXPECT_NEAR(projected->x, 640.f, 1e-2f);
    EXPECT_NEAR(projected->y, 512.f, 1e-2f);
}

TEST(unit_test_serious_engine, PredEngineTraitUsesYAsHeight)
{
    projectile_prediction::Projectile<float> projectile;
    projectile.m_origin = {0.f, 0.f, 0.f};
    projectile.m_launch_speed = 10.f;
    projectile.m_gravity_scale = 1.f;

    const auto pos = serious_engine::PredEngineTrait::predict_projectile_position(
            projectile.m_origin + projectile.m_launch_offset, projectile, 0.f, 0.f, 1.f, 9.81f);

    // heading and pitch 0 fire down -Z, and gravity pulls along -Y
    EXPECT_NEAR(pos.x, 0.f, 1e-4f);
    EXPECT_NEAR(pos.y, -9.81f * 0.5f, 1e-4f);
    EXPECT_NEAR(pos.z, -10.f, 1e-4f);
    EXPECT_NEAR(serious_engine::PredEngineTrait::get_vector_height_coordinate({1.f, 2.f, 3.f}), 2.f, 1e-4f);
}

TEST(unit_test_serious_engine, MeshTraitForwardsRotationMatrix)
{
    const auto angles = make_angles(20.f, -35.f, 15.f);

    EXPECT_EQ(serious_engine::MeshTrait::rotation_matrix(angles), serious_engine::rotation_matrix(angles));
}

// ---------------------------------------------------------------- units

TEST(unit_test_serious_engine, OneUnitIsOneMeter)
{
    EXPECT_FLOAT_EQ(serious_engine::units_to_meters(1.f), 1.f);
    EXPECT_FLOAT_EQ(serious_engine::units_to_centimeters(1.f), 100.f);
    EXPECT_DOUBLE_EQ(serious_engine::units_to_kilometers(2500.0), 2.5);

    EXPECT_FLOAT_EQ(serious_engine::meters_to_units(1.f), 1.f);
    EXPECT_FLOAT_EQ(serious_engine::centimeters_to_units(250.f), 2.5f);
    EXPECT_DOUBLE_EQ(serious_engine::kilometers_to_units(2.5), 2500.0);
}

TEST(unit_test_serious_engine, UnitConversionsRoundTrip)
{
    constexpr double units = -123456.789;

    EXPECT_NEAR(serious_engine::centimeters_to_units(serious_engine::units_to_centimeters(units)), units, 1e-6);
    EXPECT_NEAR(serious_engine::meters_to_units(serious_engine::units_to_meters(units)), units, 1e-9);
    EXPECT_NEAR(serious_engine::kilometers_to_units(serious_engine::units_to_kilometers(units)), units, 1e-6);
}

TEST(unit_test_serious_engine, UnitConversionsAreConstexprAndKeepTheirType)
{
    static_assert(serious_engine::units_to_centimeters(1.f) == 100.f);
    static_assert(std::is_same_v<decltype(serious_engine::units_to_meters(1.f)), float>);
    static_assert(std::is_same_v<decltype(serious_engine::units_to_meters(1.0)), double>);
    SUCCEED();
}
