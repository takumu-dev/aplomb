#include "rig_check.hpp"

#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/typed_id.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace aplomb::detail {

namespace {

// How far a unit vector's or a unit quaternion's squared length, or the cosine of a right angle,
// may be from exact, since a person writes them to a few decimal places. 0.7071 for √½ is within
// it.
constexpr float unit_tolerance = 1e-3F;

PartId part_at(std::size_t index) { return PartId(static_cast<std::uint32_t>(index)); }

std::string format_vector(JPH::Vec3 vector) {
    return std::format("[{}, {}, {}]", vector.GetX(), vector.GetY(), vector.GetZ());
}

std::string format_quaternion(JPH::Quat quaternion) {
    return std::format("[{}, {}, {}, {}]", quaternion.GetX(), quaternion.GetY(), quaternion.GetZ(),
                       quaternion.GetW());
}

// Names a list of parts as `"a"`, `"a" and "b"` or `"a", "b" and "c"`.
std::string list_names(const RigDesc& rig, const std::vector<PartId>& parts) {
    std::string list;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            list += index + 1 == parts.size() ? " and " : ", ";
        }
        list += std::format("\"{}\"", rig.parts[parts[index].index()].name);
    }
    return list;
}

bool is_finite(JPH::Vec3 vector) {
    return std::isfinite(vector.GetX()) && std::isfinite(vector.GetY()) &&
           std::isfinite(vector.GetZ());
}

bool is_positive_and_finite(float value) { return value > 0.0F && std::isfinite(value); }

bool is_unit(JPH::Vec3 vector) {
    return is_finite(vector) && std::abs(vector.LengthSq() - 1.0F) <= unit_tolerance;
}

bool is_unit(JPH::Quat quaternion) {
    return is_finite(quaternion.GetXYZ()) && std::isfinite(quaternion.GetW()) &&
           std::abs(quaternion.LengthSq() - 1.0F) <= unit_tolerance;
}

bool is_within(float value, float least, float greatest) {
    return value >= least && value <= greatest;
}

// Gathers the problems of one rig.
class RigChecker {
  public:
    explicit RigChecker(const RigDesc& rig) : rig_(rig), parents_(rig.parts.size()) {}

    // Checks how the parts hang together, then each part's values.
    std::expected<RigStructure, std::vector<RigProblem>> check() && {
        resolve_parents();
        check_root_parts();
        check_cycles();
        for (std::size_t part = 0; part < rig_.get().parts.size(); ++part) {
            check_part(part_at(part));
        }
        if (!problems_.empty()) {
            return std::unexpected(std::move(problems_));
        }
        return RigStructure{.parents = parents_, .parents_first = order_parents_first()};
    }

  private:
    void report(RigProblemCode code, std::optional<PartId> part, std::string message) {
        problems_.push_back({.code = code, .part = part, .message = std::move(message)});
    }

    [[nodiscard]] const std::string& name_of(PartId part) const {
        return rig_.get().parts[part.index()].name;
    }

    // Finds each part's parent by name. Where several parts share a name, a parent of that name
    // is the first of them.
    void resolve_parents() {
        std::map<std::string_view, PartId> parts_by_name;
        for (std::size_t part = 0; part < rig_.get().parts.size(); ++part) {
            if (!parts_by_name.try_emplace(rig_.get().parts[part].name, part_at(part)).second) {
                report(RigProblemCode::duplicate_part_name, part_at(part),
                       std::format("the name \"{}\" is given to more than one part",
                                   rig_.get().parts[part].name));
            }
        }
        for (std::size_t part = 0; part < rig_.get().parts.size(); ++part) {
            const std::optional<std::string>& parent = rig_.get().parts[part].parent;
            if (!parent.has_value()) {
                roots_.push_back(part_at(part));
            } else if (const auto found = parts_by_name.find(*parent);
                       found != parts_by_name.end()) {
                parents_[part] = found->second;
            } else {
                report(RigProblemCode::unknown_parent, part_at(part),
                       std::format(R"(part "{}" has the parent "{}", but no part has that name)",
                                   rig_.get().parts[part].name, *parent));
            }
        }
    }

    void check_root_parts() {
        if (roots_.empty()) {
            report(RigProblemCode::no_root_part, std::nullopt,
                   "the rig has no root part, a part without a parent, where it must have one");
        } else if (roots_.size() > 1) {
            report(RigProblemCode::several_root_parts, roots_[1],
                   std::format("the rig has {} root parts, {}, where it must have one",
                               roots_.size(), list_names(rig_, roots_)));
        }
    }

    // Follows the parents from each part in turn. A walk that comes back to a part on its own
    // path has found a cycle, which is reported once, from the part it was first entered by.
    void check_cycles() {
        enum class Visit : std::uint8_t { not_yet, on_path, done };
        std::vector<Visit> visits(rig_.get().parts.size(), Visit::not_yet);
        for (std::size_t start = 0; start < rig_.get().parts.size(); ++start) {
            std::vector<PartId> path;
            std::optional<PartId> part = part_at(start);
            while (part.has_value() && visits[part->index()] == Visit::not_yet) {
                visits[part->index()] = Visit::on_path;
                path.push_back(*part);
                part = parents_[part->index()];
            }
            if (part.has_value() && visits[part->index()] == Visit::on_path) {
                std::vector<PartId> cycle(std::ranges::find(path, *part), path.end());
                report(RigProblemCode::cycle, cycle.front(),
                       cycle.size() == 1
                           ? std::format("part \"{}\" is its own parent", name_of(cycle.front()))
                           : std::format("parts {} are each other's ancestors",
                                         list_names(rig_, cycle)));
            }
            for (const PartId visited : path) {
                visits[visited.index()] = Visit::done;
            }
        }
    }

    void check_part(PartId id) {
        const PartDesc& part = rig_.get().parts[id.index()];
        const bool is_root = !part.parent.has_value();
        if (is_root && part.joint.has_value()) {
            report(RigProblemCode::joint_on_root_part, id,
                   std::format("the root part \"{}\" has a joint, but no parent part to join",
                               part.name));
        } else if (!is_root && !part.joint.has_value()) {
            report(
                RigProblemCode::missing_joint, id,
                std::format("part \"{}\" has no joint to join it to its parent part", part.name));
        }
        if (is_root && part.muscle.has_value()) {
            report(RigProblemCode::muscle_on_root_part, id,
                   std::format("the root part \"{}\" has a muscle, but no joint for it to move",
                               part.name));
        }

        if (!is_positive_and_finite(part.mass)) {
            report(RigProblemCode::invalid_mass, id,
                   std::format("the mass of part \"{}\" must be a positive, finite number of "
                               "kilograms, not {}",
                               part.name, part.mass));
        }
        std::visit([&](const auto& shape) { check_shape(id, shape); }, part.shape);
        if (!is_finite(part.bind_position)) {
            report(RigProblemCode::non_finite_position, id,
                   std::format("the bind_position of part \"{}\" must be finite, not {}", part.name,
                               format_vector(part.bind_position)));
        }
        if (!is_unit(part.bind_rotation)) {
            report(RigProblemCode::non_unit_rotation, id,
                   std::format("the bind_rotation of part \"{}\" must be a unit quaternion, not {}",
                               part.name, format_quaternion(part.bind_rotation)));
        }
        if (part.joint.has_value()) {
            std::visit([&](const auto& joint) { check_joint(id, joint); }, *part.joint);
        }
        if (part.muscle.has_value()) {
            check_muscle(id, *part.muscle);
        }
    }

    void check_size(PartId part, std::string_view shape, std::string_view size, float value) {
        if (!is_positive_and_finite(value)) {
            report(RigProblemCode::invalid_size, part,
                   std::format("the {} of part \"{}\"'s {} must be a positive, finite number of "
                               "metres, not {}",
                               size, name_of(part), shape, value));
        }
    }

    void check_shape(PartId part, const BoxDesc& box) {
        const JPH::Vec3 half_extents = box.half_extents;
        if (!is_positive_and_finite(half_extents.GetX()) ||
            !is_positive_and_finite(half_extents.GetY()) ||
            !is_positive_and_finite(half_extents.GetZ())) {
            report(RigProblemCode::invalid_size, part,
                   std::format("the half_extents of part \"{}\"'s box must be positive, finite "
                               "numbers of metres, not {}",
                               name_of(part), format_vector(half_extents)));
        }
    }

    void check_shape(PartId part, const CapsuleDesc& capsule) {
        check_size(part, "capsule", "radius", capsule.radius);
        check_size(part, "capsule", "half_height", capsule.half_height);
    }

    void check_shape(PartId part, const SphereDesc& sphere) {
        check_size(part, "sphere", "radius", sphere.radius);
    }

    void check_anchor(PartId part, JPH::Vec3 anchor) {
        if (!is_finite(anchor)) {
            report(RigProblemCode::non_finite_position, part,
                   std::format("the anchor of part \"{}\"'s joint must be finite, not {}",
                               name_of(part), format_vector(anchor)));
        }
    }

    // Checks that a joint's two axes are unit vectors, perpendicular to each other.
    void check_axes(PartId part, std::string_view first_name, JPH::Vec3 first,
                    std::string_view second_name, JPH::Vec3 second) {
        bool both_unit = true;
        for (const auto& [name, axis] : {std::pair{first_name, first}, {second_name, second}}) {
            if (!is_unit(axis)) {
                both_unit = false;
                report(RigProblemCode::non_unit_axis, part,
                       std::format("the {} of part \"{}\"'s joint must be a unit vector, not {}",
                                   name, name_of(part), format_vector(axis)));
            }
        }
        if (both_unit && std::abs(first.Dot(second)) > unit_tolerance) {
            report(RigProblemCode::axes_not_perpendicular, part,
                   std::format("the {} and the {} of part \"{}\"'s joint must be perpendicular",
                               first_name, second_name, name_of(part)));
        }
    }

    // Checks that a joint's limit is within its range, which reads as "from -π to π".
    bool check_limit(PartId part, std::string_view limit, float value, float least, float greatest,
                     std::string_view range) {
        if (is_within(value, least, greatest)) {
            return true;
        }
        report(RigProblemCode::limit_out_of_range, part,
               std::format("the {} of part \"{}\"'s joint must be {} radians, not {}", limit,
                           name_of(part), range, value));
        return false;
    }

    void check_joint(PartId part, const SwingTwistJointDesc& swing_twist) {
        check_anchor(part, swing_twist.anchor);
        check_axes(part, "twist_axis", swing_twist.twist_axis, "plane_axis",
                   swing_twist.plane_axis);
        check_limit(part, "normal_half_cone", swing_twist.normal_half_cone, 0.0F, JPH::JPH_PI,
                    "from 0 to π");
        check_limit(part, "plane_half_cone", swing_twist.plane_half_cone, 0.0F, JPH::JPH_PI,
                    "from 0 to π");
        const bool twist_min_valid = check_limit(part, "twist_min", swing_twist.twist_min,
                                                 -JPH::JPH_PI, JPH::JPH_PI, "from -π to π");
        const bool twist_max_valid = check_limit(part, "twist_max", swing_twist.twist_max,
                                                 -JPH::JPH_PI, JPH::JPH_PI, "from -π to π");
        if (twist_min_valid && twist_max_valid && swing_twist.twist_min > swing_twist.twist_max) {
            report(RigProblemCode::limit_out_of_range, part,
                   std::format("the twist_min of part \"{}\"'s joint, {}, must be no greater "
                               "than its twist_max, {}",
                               name_of(part), swing_twist.twist_min, swing_twist.twist_max));
        }
    }

    void check_joint(PartId part, const HingeJointDesc& hinge) {
        check_anchor(part, hinge.anchor);
        check_axes(part, "hinge_axis", hinge.hinge_axis, "normal_axis", hinge.normal_axis);
        const bool limit_min_valid =
            check_limit(part, "limit_min", hinge.limit_min, -JPH::JPH_PI, 0.0F, "from -π to 0");
        const bool limit_max_valid =
            check_limit(part, "limit_max", hinge.limit_max, 0.0F, JPH::JPH_PI, "from 0 to π");
        // Jolt holds a hinge whose limits meet fixed, which a joint isn't for.
        if (limit_min_valid && limit_max_valid && hinge.limit_min == hinge.limit_max) {
            report(RigProblemCode::limit_out_of_range, part,
                   std::format("the limit_min and the limit_max of part \"{}\"'s joint must "
                               "differ, not both be 0",
                               name_of(part)));
        }
    }

    void check_muscle(PartId part, const MuscleDesc& muscle) {
        for (const auto& [setting, value] : {
                 std::pair{"stiffness", muscle.stiffness},
                 {"damping", muscle.damping},
                 {"max_torque", muscle.max_torque},
             }) {
            if (!(value >= 0.0F) || !std::isfinite(value)) {
                report(RigProblemCode::invalid_muscle_setting, part,
                       std::format("the {} of part \"{}\"'s muscle must be a finite number no "
                                   "less than 0, not {}",
                                   setting, name_of(part), value));
            }
        }
    }

    // Every part, each after its parent part: the root part, then its children, then theirs, each
    // generation in the order of the rig's parts.
    [[nodiscard]] std::vector<PartId> order_parents_first() const {
        std::vector<PartId> order{roots_.front()};
        order.reserve(parents_.size());
        for (std::size_t next = 0; next < order.size(); ++next) {
            for (std::size_t part = 0; part < parents_.size(); ++part) {
                if (parents_[part] == order[next]) {
                    order.push_back(part_at(part));
                }
            }
        }
        return order;
    }

    std::reference_wrapper<const RigDesc> rig_;
    std::vector<std::optional<PartId>> parents_;
    std::vector<PartId> roots_;
    std::vector<RigProblem> problems_;
};

} // namespace

std::expected<RigStructure, std::vector<RigProblem>> check_rig(const RigDesc& rig) {
    return RigChecker(rig).check();
}

} // namespace aplomb::detail
