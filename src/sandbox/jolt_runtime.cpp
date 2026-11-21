#include "jolt_runtime.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

JoltRuntime::JoltRuntime() {
    JPH::RegisterDefaultAllocator();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Jolt's factory is a raw global
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

JoltRuntime::~JoltRuntime() {
    JPH::UnregisterTypes();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Jolt's factory is a raw global
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
