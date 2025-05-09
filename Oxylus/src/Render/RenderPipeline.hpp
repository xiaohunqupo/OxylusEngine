#pragma once
#include <vuk/Value.hpp>

#include "Scene/ECSModule/Core.hpp"

namespace vuk {
struct SampledImage;
}

namespace ox {
class Scene;

class RenderPipeline {
public:
  struct RenderInfo {
    vuk::Extent3D extent;
    vuk::Format format;
    option<glm::uvec2> picking_texel = nullopt;
  };

  RenderPipeline(std::string name) : _name(std::move(name)) {}

  virtual ~RenderPipeline() = default;

  virtual void init(vuk::Allocator& allocator) = 0;
  virtual void shutdown() = 0;

  [[nodiscard]] virtual vuk::Value<vuk::ImageAttachment> on_render(vuk::Allocator& frame_allocator,
                                                                   const RenderInfo& render_info) = 0;

  virtual void on_update(Scene* scene) {}
  virtual void on_submit() {} // TODO: Not called anymore!! Old Code!!

  virtual void submit_mesh_component(const MeshComponent& render_object) {}
  virtual void submit_light(const LightComponent& light) {}
  virtual void submit_camera(const CameraComponent& camera) {}
  virtual void submit_sprite(const SpriteComponent& sprite) {}

  virtual const std::string& get_name() { return _name; }

protected:
  std::string _name = {};
};
} // namespace ox
