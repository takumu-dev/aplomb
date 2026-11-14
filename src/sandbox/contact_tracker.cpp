#include "contact_tracker.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>

#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

void ContactTracker::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
                                    const JPH::ContactManifold& manifold,
                                    [[maybe_unused]] JPH::ContactSettings& settings) {
    keep(body1, body2, manifold);
}

void ContactTracker::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
                                        const JPH::ContactManifold& manifold,
                                        [[maybe_unused]] JPH::ContactSettings& settings) {
    keep(body1, body2, manifold);
}

void ContactTracker::OnContactRemoved(const JPH::SubShapeIDPair& pair) {
    const std::scoped_lock lock(mutex_);
    manifolds_.erase(pair);
}

void ContactTracker::forget_body(JPH::BodyID body) {
    const std::scoped_lock lock(mutex_);
    std::erase_if(manifolds_, [body](const auto& manifold) {
        return manifold.first.GetBody1ID() == body || manifold.first.GetBody2ID() == body;
    });
}

std::vector<Contact> ContactTracker::contacts() const {
    const std::scoped_lock lock(mutex_);
    std::vector<Contact> contacts;
    for (const auto& [pair, manifold] : manifolds_) {
        contacts.insert(contacts.end(), manifold.begin(), manifold.end());
    }
    return contacts;
}

void ContactTracker::keep(const JPH::Body& body1, const JPH::Body& body2,
                          const JPH::ContactManifold& manifold) {
    std::vector<Contact> contacts;
    contacts.reserve(manifold.mRelativeContactPointsOn2.size());
    for (JPH::uint point = 0; point < manifold.mRelativeContactPointsOn2.size(); ++point) {
        contacts.push_back({
            .point = manifold.GetWorldSpaceContactPointOn2(point),
            .normal = manifold.mWorldSpaceNormal,
        });
    }
    const JPH::SubShapeIDPair pair(body1.GetID(), manifold.mSubShapeID1, body2.GetID(),
                                   manifold.mSubShapeID2);
    const std::scoped_lock lock(mutex_);
    manifolds_.insert_or_assign(pair, std::move(contacts));
}
