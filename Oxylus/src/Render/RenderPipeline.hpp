#pragma once
#include <vuk/Value.hpp>

#include "Scene/ECSModule/Core.hpp"

namespace ox {
class RenderPipeline {
public:
  struct RenderInfo {
    vuk::Extent3D extent;
    vuk::Format format;
    option<glm::uvec2> picking_texel = nullopt;
  };

  RenderPipeline(std::string name) : _name(std::move(name)) {}

  virtual ~RenderPipeline() = default;

  virtual auto init(vuk::Allocator& allocator) -> void = 0;
  virtual auto shutdown() -> void = 0;

  [[nodiscard]] virtual auto on_render(vuk::Allocator& frame_allocator,
                                       const RenderInfo& render_info) -> vuk::Value<vuk::ImageAttachment> = 0;

  virtual auto on_update(Scene* scene) -> void = 0;

  virtual auto get_name() -> const std::string& { return _name; }

protected:
  std::string _name = {};
};
} // namespace ox
