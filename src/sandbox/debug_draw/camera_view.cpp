#include "debug_draw/camera_view.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Math/Trigonometry.h>
#include <Jolt/Math/Vec3.h>

JPH::Vec3 CameraView::forward() const { return (target - eye).Normalized(); }

JPH::Vec3 CameraView::direction_through(float right, float up, float aspect) const {
    const JPH::Vec3 forward_direction = forward();
    const JPH::Vec3 right_direction = forward_direction.Cross(JPH::Vec3::sAxisY()).Normalized();
    const JPH::Vec3 up_direction = right_direction.Cross(forward_direction);
    // How far the view reaches up from its centre at a distance of 1 from the eye.
    const float half_height = JPH::Tan(0.5F * vertical_fov);
    return (forward_direction + (right * aspect * half_height * right_direction) +
            (up * half_height * up_direction))
        .Normalized();
}
