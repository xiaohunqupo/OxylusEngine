#pragma once

#include <flecs.h>

#include "Utils/Timestep.hpp"

namespace JPH {
class ContactSettings;
class ContactManifold;
class Body;
} // namespace JPH

namespace ox {
class Scene;
class System {
public:
  std::size_t hash_code = {}; // set in SystemManager::register_system

  System() = default;
  virtual ~System() = default;

  /// Called right after when the scene gets initialized.
  virtual void on_init(Scene* scene,
                       flecs::entity e) {}

  /// Called when the system is destroyed.
  virtual void on_release(Scene* scene,
                          flecs::entity e) {}

  /// Called after physic system is updated.
  virtual void on_update(const Timestep& delta_time) {}

  /// Called every fixed frame-rate frame with the frequency of the physics system
  virtual void on_fixed_update(float delta_time) {}

  /// Called after on_update
  virtual void on_render(vuk::Extent3D extent,
                         vuk::Format format) {}

  /// Called right after main loop is finished before the core shutdown process.
  virtual void on_shutdown() {}

  /// Physics interfaces
  virtual void on_contact_added(Scene* scene,
                                flecs::entity e,
                                const JPH::Body& body1,
                                const JPH::Body& body2,
                                const JPH::ContactManifold& manifold,
                                const JPH::ContactSettings& settings) {}
  virtual void on_contact_persisted(Scene* scene,
                                    flecs::entity e,
                                    const JPH::Body& body1,
                                    const JPH::Body& body2,
                                    const JPH::ContactManifold& manifold,
                                    const JPH::ContactSettings& settings) {}

  void bind_globals(this System& self,
                    Scene* scene,
                    const flecs::entity entity) {
    self._scene = scene;
    self._entity = entity;
  }

protected:
  Scene* _scene = nullptr;
  flecs::entity _entity = {};
};
} // namespace ox
