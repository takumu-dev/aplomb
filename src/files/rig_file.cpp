#include <aplomb/files/rig_file.hpp>

#include "file_reading.hpp"
#include "json_file.hpp"
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <pedantjson/pedantjson.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aplomb {

namespace {

using detail::Field;
using detail::JsonFile;

constexpr std::string_view kind = "rig file";

std::optional<JPH::Vec3> read_vector(const Field& field, JsonFile& file) {
    return detail::read_numbers<3>(field, file).transform([](const std::array<float, 3>& numbers) {
        return JPH::Vec3(numbers[0], numbers[1], numbers[2]);
    });
}

std::optional<JPH::Quat> read_quaternion(const Field& field, JsonFile& file) {
    return detail::read_numbers<4>(field, file).transform([](const std::array<float, 4>& numbers) {
        return JPH::Quat(numbers[0], numbers[1], numbers[2], numbers[3]);
    });
}

// An angle, which a rig file gives in degrees and a rig in radians.
std::optional<float> read_angle(const Field& field, JsonFile& file) {
    return detail::read_number(field, file).transform([](float degrees) {
        return JPH::DegreesToRadians(degrees);
    });
}

// Reads the structure of a rig file into a rig. Each part of the reader fills in what it can and
// reports what it can't, so that one pass finds every problem; the rig is used only if none was
// reported.
class RigFileReader {
  public:
    explicit RigFileReader(JsonFile& file) : file_(file) {}

    RigDesc read() {
        RigDesc rig;
        std::optional<detail::ObjectReader> root =
            detail::read_object(file_.get().root(), std::string(kind), file_);
        if (!root.has_value()) {
            return rig;
        }
        // A file of another format or of a newer version can't be read any further, since its
        // other keys can't be known.
        if (!detail::read_format_and_version(*root, kind, "aplomb.rig", rig_file_version, file_)
                 .has_value()) {
            return rig;
        }
        read_into(root->take("name"), rig.name, detail::read_string);
        if (const std::optional<Field> parts = root->take("parts")) {
            for (const Field& part : detail::read_array(*parts, file_).value_or({})) {
                rig.parts.push_back(read_part(part));
            }
        }
        root->finish();
        return rig;
    }

  private:
    // Reads a field's value into `out`, if it has one and it reads.
    template <typename T, typename Read>
    void read_into(const std::optional<Field>& field, T& out, Read read) {
        if (field.has_value()) {
            if (auto value = read(*field, file_.get())) {
                out = std::move(*value);
            }
        }
    }

    PartDesc read_part(const Field& field) {
        PartDesc part;
        std::optional<detail::ObjectReader> reader = detail::read_object(field, "part", file_);
        if (!reader.has_value()) {
            return part;
        }
        read_into(reader->take("name"), part.name, detail::read_string);
        // The root part says that it has no parent, so that a parent left out isn't taken for it.
        if (const std::optional<Field> parent = reader->take("parent");
            parent.has_value() && !parent->value.is_null()) {
            if (const std::optional<std::string_view> name = parent->value.as_string()) {
                part.parent = std::string(*name);
            } else {
                file_.get().report_at(*parent, FileProblemCode::wrong_type,
                                      std::format("expected a string or null, not {}",
                                                  detail::kind_name(parent->value.kind())));
            }
        }
        if (const std::optional<Field> shape = reader->take("shape")) {
            part.shape = read_shape(*shape);
        }
        read_into(reader->take("mass"), part.mass, detail::read_number);
        read_into(reader->take("bind_position"), part.bind_position, read_vector);
        read_into(reader->take("bind_rotation"), part.bind_rotation, read_quaternion);
        read_into(reader->take_optional("friction"), part.friction, detail::read_number);
        if (const std::optional<Field> joint = reader->take_optional("joint")) {
            part.joint = read_joint(*joint);
        }
        if (const std::optional<Field> muscle = reader->take_optional("muscle")) {
            part.muscle = read_muscle(*muscle);
        }
        reader->finish();
        return part;
    }

    // The type of a shape or a joint, and where the file gives it.
    struct Type {
        Field field;
        std::string name;
    };

    // Reads the type of a shape or a joint. An object whose type isn't known can't have its other
    // keys checked, so a reader that gets none doesn't finish.
    std::optional<Type> read_type(detail::ObjectReader& reader) {
        std::optional<Field> field = reader.take("type");
        if (!field.has_value()) {
            return std::nullopt;
        }
        std::optional<std::string> name = detail::read_string(*field, file_);
        if (!name.has_value()) {
            return std::nullopt;
        }
        return Type{.field = std::move(*field), .name = std::move(*name)};
    }

    void report_unknown_type(const Type& type, std::string_view noun, std::string_view known) {
        file_.get().report_at(
            type.field, FileProblemCode::invalid_value,
            std::format("the type of a {} is {}, not \"{}\"", noun, known, type.name));
    }

    ShapeDesc read_shape(const Field& field) {
        std::optional<detail::ObjectReader> reader = detail::read_object(field, "shape", file_);
        if (!reader.has_value()) {
            return {};
        }
        const std::optional<Type> type = read_type(*reader);
        if (!type.has_value()) {
            return {};
        }
        ShapeDesc shape;
        if (type->name == "box") {
            reader->rename("box shape");
            BoxDesc box;
            read_into(reader->take("half_extents"), box.half_extents, read_vector);
            shape = box;
        } else if (type->name == "capsule") {
            reader->rename("capsule shape");
            CapsuleDesc capsule;
            read_into(reader->take("radius"), capsule.radius, detail::read_number);
            read_into(reader->take("half_height"), capsule.half_height, detail::read_number);
            shape = capsule;
        } else if (type->name == "sphere") {
            reader->rename("sphere shape");
            SphereDesc sphere;
            read_into(reader->take("radius"), sphere.radius, detail::read_number);
            shape = sphere;
        } else {
            report_unknown_type(*type, "shape", R"("box", "capsule" or "sphere")");
            return {};
        }
        reader->finish();
        return shape;
    }

    JointDesc read_joint(const Field& field) {
        std::optional<detail::ObjectReader> reader = detail::read_object(field, "joint", file_);
        if (!reader.has_value()) {
            return {};
        }
        const std::optional<Type> type = read_type(*reader);
        if (!type.has_value()) {
            return {};
        }
        JointDesc joint;
        if (type->name == "swing_twist") {
            reader->rename("swing_twist joint");
            SwingTwistJointDesc swing_twist;
            read_into(reader->take("anchor"), swing_twist.anchor, read_vector);
            read_into(reader->take("twist_axis"), swing_twist.twist_axis, read_vector);
            read_into(reader->take("plane_axis"), swing_twist.plane_axis, read_vector);
            read_into(reader->take("normal_half_cone"), swing_twist.normal_half_cone, read_angle);
            read_into(reader->take("plane_half_cone"), swing_twist.plane_half_cone, read_angle);
            read_into(reader->take("twist_min"), swing_twist.twist_min, read_angle);
            read_into(reader->take("twist_max"), swing_twist.twist_max, read_angle);
            joint = swing_twist;
        } else if (type->name == "hinge") {
            reader->rename("hinge joint");
            HingeJointDesc hinge;
            read_into(reader->take("anchor"), hinge.anchor, read_vector);
            read_into(reader->take("hinge_axis"), hinge.hinge_axis, read_vector);
            read_into(reader->take("normal_axis"), hinge.normal_axis, read_vector);
            read_into(reader->take("limit_min"), hinge.limit_min, read_angle);
            read_into(reader->take("limit_max"), hinge.limit_max, read_angle);
            joint = hinge;
        } else {
            report_unknown_type(*type, "joint", R"("swing_twist" or "hinge")");
            return {};
        }
        reader->finish();
        return joint;
    }

    MuscleDesc read_muscle(const Field& field) {
        MuscleDesc muscle;
        std::optional<detail::ObjectReader> reader = detail::read_object(field, "muscle", file_);
        if (!reader.has_value()) {
            return muscle;
        }
        read_into(reader->take("stiffness"), muscle.stiffness, detail::read_number);
        read_into(reader->take("damping"), muscle.damping, detail::read_number);
        read_into(reader->take("max_torque"), muscle.max_torque, detail::read_number);
        reader->finish();
        return muscle;
    }

    std::reference_wrapper<JsonFile> file_;
};

// The JSON pointer of the value a rig problem is with. A rig's parts are in the order of the
// file's, and its members are named as the file's keys are.
pedantjson::Pointer pointer_to(const RigProblem& problem) {
    pedantjson::Pointer pointer = pedantjson::Pointer{} / "parts";
    if (problem.part.has_value()) {
        pointer = std::move(pointer) / static_cast<std::size_t>(problem.part->index());
        if (!problem.field.empty()) {
            for (const auto name : std::views::split(problem.field, '.')) {
                pointer = std::move(pointer) / std::string_view(name);
            }
        }
    }
    return pointer;
}

} // namespace

std::expected<RigDesc, FileError> load_rig_file(const std::filesystem::path& path) {
    return detail::read_text_file(path, kind).and_then([&](std::string text) {
        return parse_rig_file(std::move(text), path.string());
    });
}

std::expected<RigDesc, FileError> parse_rig_file(std::string text, std::string_view path) {
    auto document = detail::parse_json(std::move(text), path);
    if (!document.has_value()) {
        return std::unexpected(detail::invalid_file(kind, path, {std::move(document.error())}));
    }

    JsonFile structure(std::string(path), *document);
    RigDesc rig = RigFileReader(structure).read();
    if (structure.has_problems()) {
        return std::unexpected(
            detail::invalid_file(kind, path, std::move(structure).take_problems()));
    }

    JsonFile meaning(std::string(path), *document);
    for (RigProblem& problem : rig_problems(rig)) {
        const pedantjson::Pointer pointer = pointer_to(problem);
        meaning.report(FileProblemCode::invalid_rig, pointer, document->nearest(pointer).offset(),
                       std::move(problem.message), problem.code);
    }
    if (meaning.has_problems()) {
        return std::unexpected(
            detail::invalid_file(kind, path, std::move(meaning).take_problems()));
    }
    return rig;
}

} // namespace aplomb
