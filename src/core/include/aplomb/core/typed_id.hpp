/// \file
/// Typed ids, which name parts, joints and characters by their places in their tables.

#pragma once

#include <compare>
#include <cstdint>

namespace aplomb {

/// An index into one kind of table, typed so that an id of one kind can't name an object of
/// another.
///
/// \tparam Tag A type that tells one kind of id from another, and is never defined.
template <typename Tag> class TypedId {
  public:
    /// Names the object at `index` in its table.
    ///
    /// \param index The object's place in its table, counting from 0.
    constexpr explicit TypedId(std::uint32_t index) noexcept : index_(index) {}

    /// The object's place in its table, counting from 0.
    [[nodiscard]] constexpr std::uint32_t index() const noexcept { return index_; }

    /// Orders two ids by their places in their table.
    friend constexpr auto operator<=>(TypedId, TypedId) noexcept = default;

  private:
    std::uint32_t index_;
};

namespace detail {

/// Tells a part's id from the others.
struct PartIdTag;

/// Tells a joint's id from the others.
struct JointIdTag;

/// Tells a character's id from the others.
struct CharacterIdTag;

} // namespace detail

/// A part of a rig, by its place in `RigDesc::parts`.
using PartId = TypedId<detail::PartIdTag>;

/// A joint of a rig, by its place among the rig's joints. A rig's joints are in the order of the
/// parts they link to their parent parts, as those parts are in `RigDesc::parts`.
using JointId = TypedId<detail::JointIdTag>;

/// A character of a world, numbered in the order the characters were added. A world never gives a
/// removed character's id to another.
using CharacterId = TypedId<detail::CharacterIdTag>;

} // namespace aplomb
