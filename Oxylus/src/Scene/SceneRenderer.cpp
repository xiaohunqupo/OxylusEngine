#include "SceneRenderer.hpp"

#include "Scene.hpp"

#include "Core/App.hpp"

#include "Jolt/Physics/Body/Body.h"
#include "Physics/JoltHelpers.hpp"

#include "Entity.hpp"

#include "Render/DebugRenderer.hpp"
#include "Render/DefaultRenderPipeline.hpp"
#include "Render/Renderer.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Scene/Components.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
void SceneRenderer::init(EventDispatcher& dispatcher) {
  OX_SCOPED_ZONE;
  if (!_render_pipeline)
    _render_pipeline = create_shared<DefaultRenderPipeline>("DefaultRenderPipeline");
  Renderer::renderer_context.render_pipeline = _render_pipeline;
  _render_pipeline->init(*VkContext::get()->superframe_allocator);
  _render_pipeline->on_dispatcher_events(dispatcher);
}

void SceneRenderer::update() const {
  OX_SCOPED_ZONE;

  // Mesh System
  {
    OX_SCOPED_ZONE_N("Mesh System");
    const auto mesh_view = _scene->registry.view<TransformComponent, MeshComponent, TagComponent>();
    for (const auto&& [entity, transform, mesh_component, tag] : mesh_view.each()) {
      if (!tag.enabled)
        continue;

      if (!mesh_component.stationary || mesh_component.dirty) {
        const auto world_transform = eutil::get_world_transform(_scene, entity);
        mesh_component.transform = world_transform;
        for (auto& e : mesh_component.child_entities) {
          mesh_component.child_transforms.emplace_back(eutil::get_world_transform(_scene, e));
        }

        mesh_component.dirty = false;
      }

      _render_pipeline->submit_mesh_component(mesh_component);
    }
  }

  // Sprite System
  {
    OX_SCOPED_ZONE_N("Sprite System");
    const auto sprite_view = _scene->registry.view<TransformComponent, SpriteComponent, TagComponent>();
    for (const auto&& [entity, transform, sprite, tag] : sprite_view.each()) {
      if (!tag.enabled)
        continue;

      const auto world_transform = eutil::get_world_transform(_scene, entity);
      sprite.transform = world_transform;

      _render_pipeline->submit_sprite(sprite);
    }
  }

  {
    OX_SCOPED_ZONE_N("Draw physics shapes");
    if (RendererCVar::cvar_enable_debug_renderer.get() && RendererCVar::cvar_draw_physics_shapes.get()) {
      const auto mesh_collider_view = _scene->registry.view<TransformComponent, RigidbodyComponent, TagComponent>();
      for (const auto&& [entity, transform, rb, tag] : mesh_collider_view.each()) {
        if (!tag.enabled)
          continue;

        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        if (body) {
          const auto scale = JPH::Vec3{1, 1, 1}; // convert_to_jolt_vec3(transform.scale);
          auto aabb = convert_jolt_aabb(body->GetShape()->GetWorldSpaceBounds(body->GetCenterOfMassTransform(), scale));
          DebugRenderer::draw_aabb(aabb, Vec4(0, 1, 0, 1.0f));
        }
      }
    }
  }

  // Lighting
  {
    OX_SCOPED_ZONE_N("Lighting System");
    const auto lighting_view = _scene->registry.view<TransformComponent, LightComponent>();
    for (auto&& [e, tc, lc] : lighting_view.each()) {
      if (!_scene->registry.get<TagComponent>(e).enabled)
        continue;
      lc.position = tc.position;
      lc.rotation = tc.rotation;
      lc.direction = normalize(math::transform_normal(Vec4(0, 1, 0, 0), toMat4(glm::quat(tc.rotation))));

      _render_pipeline->submit_light(lc);
    }
  }

  // TODO: (very outdated, currently not working)
  // Particle system
  const auto particle_system_view = _scene->registry.view<TransformComponent, ParticleSystemComponent>();
  for (auto&& [e, tc, psc] : particle_system_view.each()) {
    psc.system->on_update((float)App::get_timestep(), tc.position);
    psc.system->on_render();
  }
}
} // namespace ox
