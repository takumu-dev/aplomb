#include "orbit_camera.hpp"

#include "debug_draw/camera_view.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Math/Math.h>
#include <Jolt/Math/Trigonometry.h>
#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <cmath>

namespace {

// Short of straight above or below, where the view's up direction would be undefined.
constexpr float max_elevation = (0.5F * JPH::JPH_PI) - 0.01F;
constexpr float min_distance = 0.5F;
constexpr float max_distance = 50.0F;
constexpr float zoom_step = 0.9F;

} // namespace

void OrbitCamera::orbit(float heading, float elevation) {
    heading_ += heading;
    elevation_ = std::clamp(elevation_ + elevation, -max_elevation, max_elevation);
}

void OrbitCamera::pan(float right, float up) {
    const JPH::Vec3 forward_direction = forward();
    const JPH::Vec3 right_direction = forward_direction.Cross(JPH::Vec3::sAxisY()).Normalized();
    const JPH::Vec3 up_direction = right_direction.Cross(forward_direction);
    target_ += distance_ * (right * right_direction + up * up_direction);
}

void OrbitCamera::zoom(float steps) {
    distance_ = std::clamp(distance_ * std::pow(zoom_step, steps), min_distance, max_distance);
}

CameraView OrbitCamera::view() const {
    return {.eye = target_ - distance_ * forward(), .target = target_};
}

JPH::Vec3 OrbitCamera::forward() const {
    const float horizontal = JPH::Cos(elevation_);
    return -JPH::Vec3(horizontal * JPH::Sin(heading_), JPH::Sin(elevation_),
                      horizontal * JPH::Cos(heading_));
}
