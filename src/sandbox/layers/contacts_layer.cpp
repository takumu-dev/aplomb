#include "layers/contacts_layer.hpp"

#include "contact_tracker.hpp"
#include "debug_draw/debug_draw.hpp"
#include "layers/marks.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>

#include <span>

namespace {

constexpr JPH::Color point_color(255, 70, 70);
constexpr JPH::Color normal_color(255, 170, 60);
constexpr float point_half_size = 0.015F;
constexpr float normal_length = 0.1F;

} // namespace

void draw_contacts_layer(std::span<const Contact> contacts, DebugDraw& draw) {
    for (const Contact& contact : contacts) {
        draw_cross(draw, contact.point, point_half_size, point_color);
        draw.overlay_line(contact.point, contact.point + (normal_length * contact.normal),
                          normal_color);
    }
}
