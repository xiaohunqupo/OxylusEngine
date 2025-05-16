#pragma once

#include "Core/System.hpp"
#include "Core/UUID.hpp"
#include "Scene/ECSModule/Core.hpp"

namespace ox {
class SceneRenderer;
class Physics3DContactListener;
class Physics3DBodyActivationListener;

struct ComponentDB {
  std::vector<flecs::id> components = {};
  std::vector<flecs::entity> imported_modules = {};

  auto import_module(this ComponentDB&,
                     flecs::entity module) -> void;
  auto is_component_known(this ComponentDB&,
                          flecs::id component_id) -> bool;
  auto get_components(this ComponentDB&) -> std::span<flecs::id>;
};

enum class SceneID : u64 { Invalid = std::numeric_limits<u64>::max() };
class Scene {
public:
  std::string scene_name = "Untitled";

  flecs::world world;
  ComponentDB component_db = {};

  Scene();
  explicit Scene(const std::string& name);

  ~Scene();

  auto init(this Scene& self,
            const std::string& name) -> void;

  auto is_running() const -> bool { return running; }

  auto create_entity(const std::string& name = "") const -> flecs::entity;

  auto on_runtime_start() -> void;
  auto on_runtime_stop() -> void;
  auto on_runtime_update(const Timestep& delta_time) -> void;

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

  auto save_to_file(this const Scene& self,
                    std::string path) -> bool;
  auto load_from_file(this Scene& self,
                      const std::string& path) -> bool;

private:
  bool running = false;

  // Renderer
  Unique<SceneRenderer> scene_renderer;

  // Physics
  Physics3DContactListener* contact_listener_3d;
  Physics3DBodyActivationListener* body_activation_listener_3d;
  float physics_frame_accumulator = 0.0f;

  // Physics
  void update_physics(const Timestep& delta_time);
};
} // namespace ox
