#include "SimpleRenderPipeline.hpp"

#include <vuk/vsl/Core.hpp>

namespace ox {
void SimpleRenderPipeline::init(vuk::Allocator& allocator) {}

void SimpleRenderPipeline::shutdown() {}

vuk::Value<vuk::ImageAttachment> SimpleRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Extent3D ext, const vuk::Format format) {
  const auto final_ia = vuk::ImageAttachment{
    .extent = ext,
    .format = format,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .level_count = 1,
    .layer_count = 1,
  };
  auto final_image = vuk::clear_image(vuk::declare_ia("final_image", final_ia), vuk::ClearColor{1.0f, 0.0f, 1.0f, 1.0f});

  return final_image;
}
} // namespace ox
