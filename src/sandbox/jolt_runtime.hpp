#pragma once

/// Jolt's process-wide setup, which the sandbox does as any host does before it creates a world:
/// the default allocator, the factory and the registered types. All but the allocator are undone
/// when it ends.
class JoltRuntime {
  public:
    JoltRuntime();
    JoltRuntime(const JoltRuntime&) = delete;
    JoltRuntime(JoltRuntime&&) = delete;
    JoltRuntime& operator=(const JoltRuntime&) = delete;
    JoltRuntime& operator=(JoltRuntime&&) = delete;
    ~JoltRuntime();
};
