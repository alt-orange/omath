local function approx(a, b, eps) return math.abs(a - b) < (eps or 1e-4) end

local function make_angles(pitch, yaw, roll)
    return omath.serious.ViewAngles.new(
        omath.serious.PitchAngle.from_degrees(pitch),
        omath.serious.YawAngle.from_degrees(yaw),
        omath.serious.RollAngle.from_degrees(roll or 0))
end

local function make_camera(angles)
    local vp  = omath.serious.ViewPort.new(1280, 1024)
    local fov = omath.serious.FieldOfView.from_degrees(90)
    return omath.serious.Camera.new(omath.Vec3.new(0, 0, 0), angles or make_angles(0, 0), vp, fov, 0.2, 1000)
end

-- Angles
function Serious_PitchAngle_clamps_to_90()
    assert(omath.serious.PitchAngle.from_degrees(100):as_degrees() == 90)
    assert(omath.serious.PitchAngle.from_degrees(-100):as_degrees() == -90)
end

function Serious_YawAngle_normalization()
    assert(approx(omath.serious.YawAngle.from_degrees(200):as_degrees(), -160))
end

-- Camera axes: Y up, looking down -Z, positive heading turns left
function Serious_Camera_default_looks_down_negative_z()
    -- get_forward() is the view matrix's third row, which points backwards (+Z) in this engine
    local back = make_camera():get_forward()
    assert(approx(back.x, 0) and approx(back.y, 0) and approx(back.z, 1))
    local up = make_camera():get_up()
    assert(approx(up.x, 0) and approx(up.y, 1) and approx(up.z, 0))
end

function Serious_Camera_positive_heading_turns_left()
    local screen, err = make_camera(make_angles(0, 90)):world_to_screen(omath.Vec3.new(-10, 0, 0))
    assert(screen ~= nil, "expected screen pos, got: " .. tostring(err))
    assert(approx(screen.x, 640, 1e-2) and approx(screen.y, 512, 1e-2))
end

-- Projection: the FOV is horizontal
function Serious_Camera_horizontal_fov_reaches_the_screen_edge()
    local cam = make_camera()
    local screen = cam:world_to_screen(omath.Vec3.new(9.99, 0, -10))
    assert(screen ~= nil)
    assert(approx(screen.x, 1280, 1))
end

function Serious_Camera_extract_projection_params()
    local fov, aspect_ratio = omath.serious.Camera.extract_projection_params(make_camera():get_projection_matrix())
    assert(approx(fov:as_degrees(), 90, 1e-3))
    assert(approx(aspect_ratio, 1280 / 1024))
end

-- View matrix round trips
function Serious_Camera_calc_view_angles_from_view_matrix()
    local view = make_camera(make_angles(-12, 118, 25)):get_view_matrix()
    local angles = omath.serious.Camera.calc_view_angles_from_view_matrix(view)
    assert(approx(angles.pitch:as_degrees(), -12, 1e-3))
    assert(approx(angles.yaw:as_degrees(), 118, 1e-3))
    assert(approx(angles.roll:as_degrees(), 25, 1e-2))
end

function Serious_Camera_calc_origin_from_view_matrix()
    local cam = make_camera()
    cam:set_origin(omath.Vec3.new(-300, 75, 410))
    local origin = omath.serious.Camera.calc_origin_from_view_matrix(cam:get_view_matrix())
    assert(approx(origin.x, -300, 1e-3) and approx(origin.y, 75, 1e-3) and approx(origin.z, 410, 1e-3))
end

-- world_to_screen / screen_to_world
function Serious_Camera_world_to_screen_center()
    local screen, err = make_camera():world_to_screen(omath.Vec3.new(0, 0, -10))
    assert(screen ~= nil, "expected screen pos, got: " .. tostring(err))
    assert(approx(screen.x, 640, 1e-2) and approx(screen.y, 512, 1e-2))
end

function Serious_Camera_world_to_screen_behind_is_error()
    local screen, err = make_camera():world_to_screen(omath.Vec3.new(0, 0, 10))
    assert(screen == nil and err ~= nil)
end

function Serious_Camera_look_at_centers_the_target()
    local cam = make_camera()
    cam:look_at(omath.Vec3.new(-40, 30, -90))
    local screen = cam:world_to_screen(omath.Vec3.new(-40, 30, -90))
    assert(screen ~= nil)
    assert(approx(screen.x, 640, 1e-2) and approx(screen.y, 512, 1e-2))
end

function Serious_Camera_screen_to_world()
    local world, err = make_camera():screen_to_world(omath.Vec3.new(640, 512, 1))
    assert(world ~= nil, "expected world pos, got: " .. tostring(err))
    assert(world.z < 0)
end
