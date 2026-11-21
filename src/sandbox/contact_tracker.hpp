#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>

#include <mutex>
#include <unordered_map>
#include <vector>

/// One point at which two bodies touch, or are about to.
struct Contact {
    /// Where the second body touches the first, in the world.
    JPH::Vec3 point = JPH::Vec3::sZero();

    /// The direction that moves the second body out of the first, as a unit vector.
    JPH::Vec3 normal = JPH::Vec3::sAxisY();
};

/// A contact listener that keeps the contacts in a physics system, from Jolt's reports of the
/// contacts it adds, keeps and removes as it steps.
///
/// Only awake bodies have contacts: Jolt reports a body's contacts as removed when it goes to
/// sleep. It reports a removed body's contacts as removed only in the step after its removal, so a
/// body's contacts are to be forgotten when it is removed.
///
/// Jolt calls the listener from its job system's threads, so each call takes a lock.
class ContactTracker final : public JPH::ContactListener {
  public:
    void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
                        const JPH::ContactManifold& manifold,
                        JPH::ContactSettings& settings) override;
    void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
                            const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override;
    void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;

    /// Forgets every contact of a body.
    ///
    /// \param body The body, which has been removed from the physics system.
    void forget_body(JPH::BodyID body);

    /// Every contact the physics system has after its last step, in no particular order.
    [[nodiscard]] std::vector<Contact> contacts() const;

  private:
    // Keeps the contacts of a pair of the bodies' shapes, in place of those it had.
    void keep(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold);

    mutable std::mutex mutex_;
    // The contacts between each pair of the bodies' shapes that touch.
    std::unordered_map<JPH::SubShapeIDPair, std::vector<Contact>> manifolds_;
};
