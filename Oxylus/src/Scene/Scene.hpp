#pragma once

#include "Components.hpp"
#include "Core/System.hpp"
#include "Core/UUID.hpp"
#include "Physics/PhysicsInterfaces.hpp"
#include "SceneRenderer.hpp"

namespace ox {
class Scene {
public:
  std::string scene_name = "Untitled";

  flecs::world world;
  flecs::entity root = {};

  Scene();
  explicit Scene(const std::string& name);

  ~Scene();

  auto init(this Scene& self,
            const std::string& name) -> void;

  auto create_entity(const std::string& name = "") const -> flecs::entity;

  auto on_runtime_start() -> void;
  auto on_runtime_stop() -> void;

  auto is_running() const -> bool { return running; }

  auto on_runtime_update(const Timestep& delta_time) -> void;
  auto on_editor_update(const Timestep& delta_time,
                        const CameraComponent& camera) const -> void;

  auto on_render(vuk::Extent3D extent,
                 vuk::Format format) -> void;

  auto has_entity(UUID uuid) const -> bool;
  static auto copy(const Shared<Scene>& src_scene) -> Shared<Scene>;

  auto get_world_transform(flecs::entity entity) const -> glm::mat4;
  auto get_local_transform(flecs::entity entity) const -> glm::mat4;

  // Physics interfaces
  auto on_contact_added(const JPH::Body& body1,
                        const JPH::Body& body2,
                        const JPH::ContactManifold& manifold,
                        const JPH::ContactSettings& settings) -> void;
  auto on_contact_persisted(const JPH::Body& body1,
                            const JPH::Body& body2,
                            const JPH::ContactManifold& manifold,
                            const JPH::ContactSettings& settings) -> void;

  auto create_rigidbody(flecs::entity entity,
                        const TransformComponent& transform,
                        RigidbodyComponent& component) -> void;
  auto create_character_controller(const TransformComponent& transform,
                                   CharacterControllerComponent& component) const -> void;

  // Renderer
  auto get_renderer() -> const Unique<SceneRenderer>& { return scene_renderer; }

  auto save_to_file(this Scene& self,
                    std::string path) -> bool;
  auto load_from_file(this Scene& self,
                      const std::string& path) -> bool;

private:
  bool running = false;

  // Renderer
  Unique<SceneRenderer> scene_renderer = nullptr;

  // Physics
  Physics3DContactListener* contact_listener_3d = nullptr;
  Physics3DBodyActivationListener* body_activation_listener_3d = nullptr;
  float physics_frame_accumulator = 0.0f;

  // Physics
  void update_physics(const Timestep& delta_time);

  friend class SceneSerializer;
  friend class SceneHPanel;
};
} // namespace ox
