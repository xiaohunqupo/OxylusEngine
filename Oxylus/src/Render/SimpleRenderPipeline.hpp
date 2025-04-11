#pragma once
#include "RenderPipeline.hpp"

namespace ox {
class SimpleRenderPipeline : public RenderPipeline {
public:
  explicit SimpleRenderPipeline(const std::string& name) : RenderPipeline(name) {}
  ~SimpleRenderPipeline() override = default;

  void init(vuk::Allocator& allocator) override;
  void shutdown() override;
  [[nodiscard]] vuk::Value<vuk::ImageAttachment> on_render(vuk::Allocator& frame_allocator, vuk::Extent3D ext, vuk::Format format) override;
};
} // namespace ox
