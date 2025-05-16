#include "SceneRenderer.hpp"

#include "Asset/AssetManager.hpp"
#include "Core/App.hpp"
#include "Physics/Physics.hpp"
#include "Render/Camera.hpp"
#include "Render/DebugRenderer.hpp"
#include "Render/EasyRenderPipeline.hpp"
#include "Render/RendererConfig.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Scene.hpp"
#include "Scene/ECSModule/Core.hpp"

namespace ox {
void SceneRenderer::init(Scene* scene,
                         const Shared<RenderPipeline>& render_pipeline) {
  OX_SCOPED_ZONE;

  _scene = scene;

  if (!render_pipeline)
    _render_pipeline = create_unique<EasyRenderPipeline>("EasyRenderPipeline");
  else
    _render_pipeline = render_pipeline;

  _render_pipeline->init(*App::get_vkcontext().superframe_allocator);
}

void SceneRenderer::update(const Timestep& delta_time) const {
  OX_SCOPED_ZONE;

  // Camera
  {
    OX_SCOPED_ZONE_N("Camera System");
    _scene->world.query_builder<const TransformComponent, CameraComponent>().build().each(
        [](const TransformComponent& tc, CameraComponent& cc) {
      const auto screen_extent = App::get()->get_swapchain_extent();
      cc.position = tc.position;
      cc.pitch = tc.rotation.x;
      cc.yaw = tc.rotation.y;
      Camera::update(cc, screen_extent);
    });
  }

  {
    OX_SCOPED_ZONE_N("Mesh System");
    _scene->world.query_builder<const TransformComponent, MeshComponent>().build().each(
        [](const TransformComponent& tc, MeshComponent& cc) {});
#if 0
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
#endif
  }

  {
    OX_SCOPED_ZONE_N("Sprite Animation System");
    _scene->world.query_builder<SpriteComponent, SpriteAnimationComponent>().build().each(
        [&delta_time](SpriteComponent& sprite, SpriteAnimationComponent& sprite_animation) {
      const auto asset_manager = App::get_system<AssetManager>(EngineSystems::AssetManager);
      auto* material = asset_manager->get_material(sprite.material);

      if (sprite_animation.num_frames < 1 || sprite_animation.fps < 1 || sprite_animation.columns < 1 || !material ||
          !material->albedo_texture)
        return;

      const auto dt = glm::clamp(static_cast<float>(delta_time.get_seconds()), 0.0f, 0.25f);
      const auto time = sprite_animation.current_time + dt;

      sprite_animation.current_time = time;

      const float duration = static_cast<float>(sprite_animation.num_frames) / sprite_animation.fps;
      u32 frame = math::flooru32(sprite_animation.num_frames * (time / duration));

      if (time > duration) {
        if (sprite_animation.inverted) {
          // Remove/add a frame depending on the direction
          const float frame_length = 1.0f / sprite_animation.fps;
          sprite_animation.current_time -= duration - frame_length;
        } else {
          sprite_animation.current_time -= duration;
        }
      }

      if (sprite_animation.loop)
        frame %= sprite_animation.num_frames;
      else
        frame = glm::min(frame, sprite_animation.num_frames - 1);

      frame = sprite_animation.inverted ? sprite_animation.num_frames - 1 - frame : frame;

      const u32 frame_x = frame % sprite_animation.columns;
      const u32 frame_y = frame / sprite_animation.columns;

      const auto* albedo_texture = asset_manager->get_texture(material->albedo_texture);
      auto& uv_size = material->uv_size;

      auto texture_size = glm::vec2(albedo_texture->get_extent().width, albedo_texture->get_extent().height);
      uv_size = {sprite_animation.frame_size[0] * 1.f / texture_size[0],
                 sprite_animation.frame_size[1] * 1.f / texture_size[1]};
      sprite.current_uv_offset = material->uv_offset + glm::vec2{uv_size.x * frame_x, uv_size.y * frame_y};
    });
  }

  {
    OX_SCOPED_ZONE_N("Sprite System");
    _scene->world.query_builder<SpriteComponent>().build().each(
        [this](const flecs::entity entity, SpriteComponent& sprite) {
      const auto world_transform = _scene->get_world_transform(entity);
      sprite.transform = world_transform;
      sprite.rect = AABB(glm::vec3(-0.5, -0.5, -0.5), glm::vec3(0.5, 0.5, 0.5));
      sprite.rect = sprite.rect.get_transformed(world_transform);

      if (RendererCVar::cvar_draw_bounding_boxes.get()) {
        DebugRenderer::draw_aabb(sprite.rect, glm::vec4(1, 1, 1, 1.0f));
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("Physics Debug Renderer");
    if (RendererCVar::cvar_enable_physics_debug_renderer.get()) {
      auto physics = App::get_system<Physics>(EngineSystems::Physics);
      physics->debug_draw();
    }
  }

  {
    OX_SCOPED_ZONE_N("Lighting System");
    _scene->world.query_builder<const TransformComponent, LightComponent>().build().each(
        [](const TransformComponent& tc, LightComponent& lc) {
      lc.position = tc.position;
      lc.rotation = tc.rotation;
      lc.direction = glm::normalize(math::transform_normal(glm::vec4(0, 1, 0, 0), toMat4(glm::quat(tc.rotation))));
    });
  }

  _render_pipeline->on_update(_scene);
}
} // namespace ox
